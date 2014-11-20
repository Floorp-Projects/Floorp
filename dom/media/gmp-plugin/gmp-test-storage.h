/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_GMP_STORAGE_H__
#define TEST_GMP_STORAGE_H__

#include "gmp-errors.h"
#include "gmp-platform.h"
#include <string>

class ReadContinuation {
public:
  virtual ~ReadContinuation() {}
  virtual void ReadComplete(GMPErr aErr, const std::string& aData) = 0;
};

// Reads a record to storage using GMPRecord.
// Calls ReadContinuation with read data.
GMPErr
ReadRecord(const std::string& aRecordName,
           ReadContinuation* aContinuation);

// Writes a record to storage using GMPRecord.
// Runs continuation when data is written.
GMPErr
WriteRecord(const std::string& aRecordName,
            const std::string& aData,
            GMPTask* aContinuation);

GMPErr
WriteRecord(const std::string& aRecordName,
            const uint8_t* aData,
            uint32_t aNumBytes,
            GMPTask* aContinuation);

GMPErr
GMPOpenRecord(const char* aName,
              uint32_t aNameLength,
              GMPRecord** aOutRecord,
              GMPRecordClient* aClient);

GMPErr
GMPRunOnMainThread(GMPTask* aTask);

class OpenContinuation {
public:
  virtual ~OpenContinuation() {}
  virtual void OpenComplete(GMPErr aStatus, GMPRecord* aRecord) = 0;
};

GMPErr
GMPOpenRecord(const std::string& aRecordName,
              OpenContinuation* aContinuation);

GMPErr
GMPEnumRecordNames(RecvGMPRecordIteratorPtr aRecvIteratorFunc,
                   void* aUserArg);

#endif // TEST_GMP_STORAGE_H__
