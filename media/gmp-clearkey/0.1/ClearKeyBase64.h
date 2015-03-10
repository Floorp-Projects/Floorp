/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeyBase64_h__
#define __ClearKeyBase64_h__

#include <vector>
#include <string>
#include <stdint.h>

// Decodes a base64 encoded CENC Key or KeyId into it's raw bytes. Note that
// CENC Keys or KeyIds are 16 bytes long, so encoded they should be 22 bytes
// plus any padding. Fails (returns false) on input that is more than 22 bytes
// long after padding is stripped. Returns true on success.
bool
DecodeBase64KeyOrId(const std::string& aEncoded, std::vector<uint8_t>& aOutDecoded);

#endif
