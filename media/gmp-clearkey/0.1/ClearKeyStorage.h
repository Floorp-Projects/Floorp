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

#ifndef __ClearKeyStorage_h__
#define __ClearKeyStorage_h__

#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

#include "ClearKeySessionManager.h"

#define IO_SUCCEEDED(x) ((x) == cdm::FileIOClient::Status::kSuccess)
#define IO_FAILED(x) ((x) != cdm::FileIOClient::Status::kSuccess)

// Writes data to a file and fires the appropriate callback when complete.
void WriteData(cdm::Host_8* aHost,
               std::string& aRecordName,
               const std::vector<uint8_t>& aData,
               std::function<void()>&& aOnSuccess,
               std::function<void()>&& aOnFailure);

// Reads data from a file and fires the appropriate callback when complete.
void ReadData(cdm::Host_8* aHost,
              std::string& aRecordName,
              std::function<void(const uint8_t*, uint32_t)>&& aOnSuccess,
              std::function<void()>&& aOnFailure);

#endif // __ClearKeyStorage_h__
