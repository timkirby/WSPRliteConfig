#include "dxplorer.hpp"
#include "common/StrUtil.hpp"
#include "common/hash.hpp"
#include "base64_url_unpadded.hpp"

#include <cctype>
#include <algorithm>
#include <iostream>

static uint8_t storeInt(std::string &dest, uint64_t data)
{
	int byteCount = 0;
	do
	{
		dest.push_back(uint8_t(data&0xFF));
		data >>= 8;
		byteCount++;
	}
	while (data!=0);
	return byteCount-1;
}


std::string DXplorer::generateKey(uint64_t deviceId, std::vector<uint8_t> deviceSecret, uint64_t changeCounter, std::string callsign)
{
	callsign = StrUtil::toUpper(callsign);

	/* Taking + to mean concatenation, the key is generated as:
	 *  key = (lengths of next two parts packed into a uint8, 4 bits each)
	 *    + (as many bytes as necessary of deviceId, least significant first)
	 *    + (as many bytes as necessary of changeCounter, least significant first)
	 *    + (hash - see below)
	 */

	std::string key = "";
	uint8_t packedCounts = 0;
	// Reserve space for packedCounts
	key.push_back(0);

	packedCounts |= storeInt(key, deviceId);
	packedCounts |= storeInt(key, changeCounter)<<3;

	key[0] = packedCounts;

	/* hash = sha256(deviceId+changeCounter+callsign+secret data)   (numbers formatted as strings)
	 * On DXplorer.net, there is a database table containing device IDs and 'secret data' for each device ID.
	 * 'secret data' is just some randomly generated bytes, and is called secret because it never
	 * appears directly in a URL. A hash based on this secret data proves that the user has access to some
	 * hardware in which this secret data resided (e.g. a WSPRlite).
	 * Only the latest key (as determined by changeCounter) for each device ID is allowed access. If multiple
	 * keys with the same changeCounter are seen, only the first one seen is allowed.
	 *
	 * Note that use of the generated keys to access DXplorer is logged, and sharing secret data with
	 * other people may result in access being temporarily or permanently suspended if they use it to generate
	 * their own keys without owning the relevant hardware. Secret data should not leave the config program.
	 *
	 * Generated keys may be shared, it is only the secret data which allows more keys to be generated that
	 * should not be shared.
	 */
	std::vector<uint8_t> hashInput;
	std::string hashInputStr = std::to_string(deviceId) + std::to_string(changeCounter) + callsign;
	std::copy(hashInputStr.begin(), hashInputStr.end(), std::back_inserter(hashInput));
	std::copy(deviceSecret.begin(), deviceSecret.end(), std::back_inserter(hashInput));
	std::string hashOutputHex = sha256(hashInput);
	std::vector<uint8_t> hashOutput = StrUtil::hex_decode(hashOutputHex);
	std::copy(hashOutput.begin(), hashOutput.begin()+16, std::back_inserter(key));

	return cppcodec::base64_url_unpadded::encode(key);
}
