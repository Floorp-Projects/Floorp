#include "WidevineFileIO.h"
#include "GMPLog.h"
#include "WidevineUtils.h"

#include "gmp-api/gmp-platform.h"

using namespace cdm;

// Declared in ChromiumCDMAdapter.cpp.
extern const GMPPlatformAPI* sPlatform;

namespace mozilla {

void
WidevineFileIO::Open(const char* aFilename, uint32_t aFilenameLength)
{
  mName = std::string(aFilename, aFilename + aFilenameLength);
  GMPRecord* record = nullptr;
  GMPErr err = sPlatform->createrecord(aFilename, aFilenameLength, &record, static_cast<GMPRecordClient*>(this));
  if (GMP_FAILED(err)) {
    GMP_LOG("WidevineFileIO::Open() '%s' GMPCreateRecord failed",
            mName.c_str());
    mClient->OnOpenComplete(FileIOClient::kError);
    return;
  }
  if (GMP_FAILED(record->Open())) {
    GMP_LOG("WidevineFileIO::Open() '%s' record open failed", mName.c_str());
    mClient->OnOpenComplete(FileIOClient::kError);
    return;
  }

  GMP_LOG("WidevineFileIO::Open() '%s'", mName.c_str());
  mRecord = record;
}

void
WidevineFileIO::Read()
{
  if (!mRecord) {
    GMP_LOG("WidevineFileIO::Read() '%s' used uninitialized!", mName.c_str());
    mClient->OnReadComplete(FileIOClient::kError, nullptr, 0);
    return;
  }
  GMP_LOG("WidevineFileIO::Read() '%s'", mName.c_str());
  mRecord->Read();
}

void
WidevineFileIO::Write(const uint8_t* aData, uint32_t aDataSize)
{
  if (!mRecord) {
    GMP_LOG("WidevineFileIO::Write() '%s' used uninitialized!", mName.c_str());
    mClient->OnWriteComplete(FileIOClient::kError);
    return;
  }
  mRecord->Write(aData, aDataSize);
}

void
WidevineFileIO::Close()
{
  GMP_LOG("WidevineFileIO::Close() '%s'", mName.c_str());
  if (mRecord) {
    mRecord->Close();
    mRecord = nullptr;
  }
  delete this;
}

static FileIOClient::Status
GMPToWidevineFileStatus(GMPErr aStatus)
{
  switch (aStatus) {
    case GMPRecordInUse: return FileIOClient::kInUse;
    case GMPNoErr: return FileIOClient::kSuccess;
    default: return FileIOClient::kError;
  }
}

void
WidevineFileIO::OpenComplete(GMPErr aStatus)
{
  GMP_LOG(
    "WidevineFileIO::OpenComplete() '%s' status=%d", mName.c_str(), aStatus);
  mClient->OnOpenComplete(GMPToWidevineFileStatus(aStatus));
}

void
WidevineFileIO::ReadComplete(GMPErr aStatus,
                             const uint8_t* aData,
                             uint32_t aDataSize)
{
  GMP_LOG(
    "WidevineFileIO::OnReadComplete() '%s' status=%d", mName.c_str(), aStatus);
  mClient->OnReadComplete(GMPToWidevineFileStatus(aStatus), aData, aDataSize);
}

void
WidevineFileIO::WriteComplete(GMPErr aStatus)
{
  GMP_LOG(
    "WidevineFileIO::WriteComplete() '%s' status=%d", mName.c_str(), aStatus);
  mClient->OnWriteComplete(GMPToWidevineFileStatus(aStatus));
}

} // namespace mozilla
