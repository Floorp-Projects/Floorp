/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_CDM_STORAGE_H__
#define TEST_CDM_STORAGE_H__

#include <functional>
#include <string>
#include <vector>
// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"

#define IO_SUCCEEDED(x) ((x) == cdm::FileIOClient::Status::kSuccess)
#define IO_FAILED(x) ((x) != cdm::FileIOClient::Status::kSuccess)

class ReadContinuation {
public:
  virtual ~ReadContinuation() {}
  virtual void operator()(bool aSuccess,
                          const uint8_t* aData,
                          uint32_t aDataSize) = 0;
};

void WriteRecord(cdm::Host_8* aHost,
                 const std::string& aRecordName,
                 const std::string& aData,
                 std::function<void()>&& aOnSuccess,
                 std::function<void()>&& aOnFailure);

void WriteRecord(cdm::Host_8* aHost,
                 const std::string& aRecordName,
                 const uint8_t* aData,
                 uint32_t aNumBytes,
                 std::function<void()>&& aOnSuccess,
                 std::function<void()>&& aOnFailure);

void ReadRecord(cdm::Host_8* aHost,
                const std::string& aRecordName,
                std::function<void(bool, const uint8_t*, uint32_t)>&& aOnReadComplete);

class OpenContinuation {
public:
  virtual ~OpenContinuation() {}
  virtual void operator()(bool aSuccess) = 0;
};

void OpenRecord(cdm::Host_8* aHost,
                const std::string& aRecordName,
                std::function<void(bool)>&& aOpenComplete);
#endif // TEST_CDM_STORAGE_H__
