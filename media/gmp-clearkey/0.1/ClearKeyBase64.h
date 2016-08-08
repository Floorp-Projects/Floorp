/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
