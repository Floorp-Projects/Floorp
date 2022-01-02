/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineFileIO.h"
#include "GMPLog.h"
#include "WidevineUtils.h"

#include "gmp-api/gmp-platform.h"

// Declared in ChromiumCDMAdapter.cpp.
extern const GMPPlatformAPI* sPlatform;

namespace mozilla {

void WidevineFileIO::Open(const char* aFilename, uint32_t aFilenameLength) {
  mName = std::string(aFilename, aFilename + aFilenameLength);
  GMPRecord* record = nullptr;
  GMPErr err = sPlatform->createrecord(aFilename, aFilenameLength, &record,
                                       static_cast<GMPRecordClient*>(this));
  if (GMP_FAILED(err)) {
    GMP_LOG_DEBUG("WidevineFileIO::Open() '%s' GMPCreateRecord failed",
                  mName.c_str());
    mClient->OnOpenComplete(cdm::FileIOClient::Status::kError);
    return;
  }
  if (GMP_FAILED(record->Open())) {
    GMP_LOG_DEBUG("WidevineFileIO::Open() '%s' record open failed",
                  mName.c_str());
    mClient->OnOpenComplete(cdm::FileIOClient::Status::kError);
    return;
  }

  GMP_LOG_DEBUG("WidevineFileIO::Open() '%s'", mName.c_str());
  mRecord = record;
}

void WidevineFileIO::Read() {
  if (!mRecord) {
    GMP_LOG_DEBUG("WidevineFileIO::Read() '%s' used uninitialized!",
                  mName.c_str());
    mClient->OnReadComplete(cdm::FileIOClient::Status::kError, nullptr, 0);
    return;
  }
  GMP_LOG_DEBUG("WidevineFileIO::Read() '%s'", mName.c_str());
  mRecord->Read();
}

void WidevineFileIO::Write(const uint8_t* aData, uint32_t aDataSize) {
  if (!mRecord) {
    GMP_LOG_DEBUG("WidevineFileIO::Write() '%s' used uninitialized!",
                  mName.c_str());
    mClient->OnWriteComplete(cdm::FileIOClient::Status::kError);
    return;
  }
  mRecord->Write(aData, aDataSize);
}

void WidevineFileIO::Close() {
  GMP_LOG_DEBUG("WidevineFileIO::Close() '%s'", mName.c_str());
  if (mRecord) {
    mRecord->Close();
    mRecord = nullptr;
  }
  delete this;
}

static cdm::FileIOClient::Status GMPToWidevineFileStatus(GMPErr aStatus) {
  switch (aStatus) {
    case GMPRecordInUse:
      return cdm::FileIOClient::Status::kInUse;
    case GMPNoErr:
      return cdm::FileIOClient::Status::kSuccess;
    default:
      return cdm::FileIOClient::Status::kError;
  }
}

void WidevineFileIO::OpenComplete(GMPErr aStatus) {
  GMP_LOG_DEBUG("WidevineFileIO::OpenComplete() '%s' status=%d", mName.c_str(),
                aStatus);
  mClient->OnOpenComplete(GMPToWidevineFileStatus(aStatus));
}

void WidevineFileIO::ReadComplete(GMPErr aStatus, const uint8_t* aData,
                                  uint32_t aDataSize) {
  GMP_LOG_DEBUG("WidevineFileIO::OnReadComplete() '%s' status=%d",
                mName.c_str(), aStatus);
  mClient->OnReadComplete(GMPToWidevineFileStatus(aStatus), aData, aDataSize);
}

void WidevineFileIO::WriteComplete(GMPErr aStatus) {
  GMP_LOG_DEBUG("WidevineFileIO::WriteComplete() '%s' status=%d", mName.c_str(),
                aStatus);
  mClient->OnWriteComplete(GMPToWidevineFileStatus(aStatus));
}

}  // namespace mozilla
