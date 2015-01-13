#include "SpectrumSensor.hpp"

#include <boost/regex.hpp>

#include <stdio.h>

using namespace VESNA;

void ConfigList::parse(std::vector<std::string> lines)
{
	std::vector<std::string>::iterator line = lines.begin();

	boost::regex rdevice("device ([0-9]+): (.*)\n");
	boost::regex rdevsam("  device supports channel sampling\n");
	boost::regex rconfig("  channel config [0-9]+,([0-9]+): (.*)\n");
	boost::regex rcparam("    ([a-z]+): ([0-9]+).*");

	for(; line != lines.end(); ++line) {
		boost::smatch g;
		if(boost::regex_match(*line, g, rdevice)) {
			int id = atoi(std::string(g[1]).c_str());
			std::string name = g[2];

			Device device(id, name);
			devices.push_back(device);
		} else if(boost::regex_match(*line, g, rdevsam)) {
			devices.rbegin()->supports_sampling = true;
		} else if(boost::regex_match(*line, g, rconfig)) {

			int id = atoi(std::string(g[1]).c_str());
			std::string name = g[2];

			DeviceConfig config(id, name, &(*devices.rbegin()));
			configs.push_back(config);
		} else if(boost::regex_match(*line, g, rcparam)) {
			std::string param = g[1];

			long long value = atoll(std::string(g[2]).c_str());

			if(!param.compare("base")) {
				configs.rbegin()->base = value;
			} else if(!param.compare("spacing")) {
				configs.rbegin()->spacing = value;
			} else if(!param.compare("num")) {
				configs.rbegin()->num = value;
			} else if(!param.compare("bw")) {
				configs.rbegin()->bw = value;
			} else if(!param.compare("time")) {
				configs.rbegin()->time = value;
			}
		}
	}
}

DeviceConfig* ConfigList::get_config(int device_id, int config_id)
{
	std::vector<DeviceConfig>::iterator i = configs.begin();
	for(; i != configs.end(); ++i) {
		if(i->id == config_id && i->device->id == device_id) {
			return &(*i);
		}
	}

	return NULL;
}

hz_t DeviceConfig::ch_to_hz(ch_t ch)
{
	assert(ch >= 0);
	assert(ch < num);

	return base + spacing * ch;
}

ch_t DeviceConfig::hz_to_ch(hz_t hz)
{
	assert(covers(hz, hz));
	return (hz - base) / spacing;
}

hz_t DeviceConfig::get_start_hz()
{
	return ch_to_hz(0);
}

hz_t DeviceConfig::get_stop_hz()
{
	return ch_to_hz(num - 1);
}

bool DeviceConfig::covers(hz_t start_hz, hz_t stop_hz)
{
	return (start_hz >= get_start_hz()) && (stop_hz <= get_stop_hz());
}

SweepConfig* DeviceConfig::get_sample_config(hz_t hz, int nsamples)
{
	ch_t ch = hz_to_ch(hz);

	return new SweepConfig(this, ch, ch+1, 1, nsamples);
}

SpectrumSensor::SpectrumSensor(const std::string &port)
{
	comm = new serial::Serial(port, 576000, serial::Timeout::simpleTimeout(1000));
	
	comm->write("sweep-off\n");
	wait_for_ok();
	comm->write("sample-off\n");
	wait_for_ok();
}

SpectrumSensor::~SpectrumSensor()
{
	delete comm;
}

ConfigList* SpectrumSensor::get_config_list()
{
	std::vector<std::string> lines;

	comm->write("list\n");

	while(1) {
		std::string r = comm->readline();
		if(r.length() > 0) {
			lines.push_back(r);
		} else {
			break;
		}
	}

	ConfigList* cl = new ConfigList();
	cl->parse(lines);

	return cl;
}

void SpectrumSensor::wait_for_ok()
{
	while(1) {
		std::string r = comm->readline();
		if(!r.compare("ok\n")) {
			return;
		}
	}
}

void SpectrumSensor::select_sweep_channel(SweepConfig* sc)
{
	const int buffer_size = 1024;
	char buffer[buffer_size];

	snprintf(buffer, buffer_size, "select channel %lld:%lld:%lld config %d,%d\n",
			sc->start_ch, sc->step_ch, sc->stop_ch, sc->config->device->id, sc->config->id);

	comm->write(buffer);

	wait_for_ok();
}
