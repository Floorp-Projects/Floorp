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

#include "gmp-api/gmp-errors.h"
#include "gmp-api/gmp-platform.h"
#include <string>
#include <vector>
#include <stdint.h>

class GMPTask;

// Responsible for ensuring that both aOnSuccess and aOnFailure are destroyed.
void StoreData(const std::string& aRecordName,
               const std::vector<uint8_t>& aData,
               GMPTask* aOnSuccess,
               GMPTask* aOnFailure);

class ReadContinuation {
public:
  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aLength) = 0;
  virtual ~ReadContinuation() {}
};

// Deletes aContinuation after running it to report the result.
void ReadData(const std::string& aSessionId,
              ReadContinuation* aContinuation);

GMPErr EnumRecordNames(RecvGMPRecordIteratorPtr aRecvIteratorFunc);

#endif // __ClearKeyStorage_h__
