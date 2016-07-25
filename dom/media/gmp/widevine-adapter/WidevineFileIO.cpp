#include "WidevineFileIO.h"
#include "WidevineUtils.h"
#include "WidevineAdapter.h"

using namespace cdm;

namespace mozilla {

void
WidevineFileIO::Open(const char* aFilename, uint32_t aFilenameLength)
{
  mName = std::string(aFilename, aFilename + aFilenameLength);
  GMPRecord* record = nullptr;
  GMPErr err = GMPCreateRecord(aFilename, aFilenameLength, &record, static_cast<GMPRecordClient*>(this));
  if (GMP_FAILED(err)) {
    Log("WidevineFileIO::Open() '%s' GMPCreateRecord failed", mName.c_str());
    mClient->OnOpenComplete(FileIOClient::kError);
    return;
  }
  if (GMP_FAILED(record->Open())) {
    Log("WidevineFileIO::Open() '%s' record open failed", mName.c_str());
    mClient->OnOpenComplete(FileIOClient::kError);
    return;
  }

  Log("WidevineFileIO::Open() '%s'", mName.c_str());
  mRecord = record;
}

void
WidevineFileIO::Read()
{
  if (!mRecord) {
    Log("WidevineFileIO::Read() '%s' used uninitialized!", mName.c_str());
    mClient->OnReadComplete(FileIOClient::kError, nullptr, 0);
    return;
  }
  Log("WidevineFileIO::Read() '%s'", mName.c_str());
  mRecord->Read();
}

void
WidevineFileIO::Write(const uint8_t* aData, uint32_t aDataSize)
{
  if (!mRecord) {
    Log("WidevineFileIO::Write() '%s' used uninitialized!", mName.c_str());
    mClient->OnWriteComplete(FileIOClient::kError);
    return;
  }
  mRecord->Write(aData, aDataSize);
}

void
WidevineFileIO::Close()
{
  Log("WidevineFileIO::Close() '%s'", mName.c_str());
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
  Log("WidevineFileIO::OpenComplete() '%s' status=%d", mName.c_str(), aStatus);
  mClient->OnOpenComplete(GMPToWidevineFileStatus(aStatus));
}

void
WidevineFileIO::ReadComplete(GMPErr aStatus,
                             const uint8_t* aData,
                             uint32_t aDataSize)
{
  Log("WidevineFileIO::OnReadComplete() '%s' status=%d", mName.c_str(), aStatus);
  mClient->OnReadComplete(GMPToWidevineFileStatus(aStatus), aData, aDataSize);
}

void
WidevineFileIO::WriteComplete(GMPErr aStatus)
{
  Log("WidevineFileIO::WriteComplete() '%s' status=%d", mName.c_str(), aStatus);
  mClient->OnWriteComplete(GMPToWidevineFileStatus(aStatus));
}

} // namespace mozilla
