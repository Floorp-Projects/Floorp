/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidevineFileIO_h_
#define WidevineFileIO_h_

#include <stddef.h>
#include "content_decryption_module.h"
#include "gmp-api/gmp-storage.h"
#include <string>

namespace mozilla {

class WidevineFileIO : public cdm::FileIO
                     , public GMPRecordClient
{
public:
  explicit WidevineFileIO(cdm::FileIOClient* aClient)
    : mClient(aClient)
    , mRecord(nullptr)
  {}

  // cdm::FileIO
  void Open(const char* aFilename, uint32_t aFilenameLength) override;
  void Read() override;
  void Write(const uint8_t* aData, uint32_t aDataSize) override;
  void Close() override;

  // GMPRecordClient
  void OpenComplete(GMPErr aStatus) override;
  void ReadComplete(GMPErr aStatus,
                    const uint8_t* aData,
                    uint32_t aDataSize) override;
  void WriteComplete(GMPErr aStatus) override;

private:
  cdm::FileIOClient* mClient;
  GMPRecord* mRecord;
  std::string mName;
};

} // namespace mozilla

#endif // WidevineFileIO_h_