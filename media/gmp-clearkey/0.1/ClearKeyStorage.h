/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeyStorage_h__
#define __ClearKeyStorage_h__

#include "gmp-errors.h"
#include "gmp-platform.h"
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
