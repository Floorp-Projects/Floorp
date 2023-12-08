/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cdm-test-storage.h"
#include <vector>

using namespace cdm;

class WriteRecordClient : public FileIOClient {
 public:
  WriteRecordClient(std::function<void()>&& aOnSuccess,
                    std::function<void()>&& aOnFailure, const uint8_t* aData,
                    uint32_t aDataSize)
      : mOnSuccess(std::move(aOnSuccess)), mOnFailure(std::move(aOnFailure)) {
    mData.insert(mData.end(), aData, aData + aDataSize);
  }

  void OnOpenComplete(Status aStatus) override {
    // If we hit an error, fail.
    if (aStatus != Status::kSuccess) {
      Done(aStatus);
    } else if (mFileIO) {  // Otherwise, write our data to the file.
      mFileIO->Write(mData.empty() ? nullptr : &mData.front(), mData.size());
    }
  }

  void OnReadComplete(Status aStatus, const uint8_t* aData,
                      uint32_t aDataSize) override {}

  void OnWriteComplete(Status aStatus) override { Done(aStatus); }

  void Do(const std::string& aName, Host_10* aHost) {
    // Initialize the FileIO.
    mFileIO = aHost->CreateFileIO(this);
    mFileIO->Open(aName.c_str(), aName.size());
  }

 private:
  void Done(cdm::FileIOClient::Status aStatus) {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mFileIO) {
      // will delete mFileIO inside Close.
      mFileIO->Close();
    }

    if (IO_SUCCEEDED(aStatus)) {
      mOnSuccess();
    } else {
      mOnFailure();
    }

    delete this;
  }

  FileIO* mFileIO = nullptr;
  std::function<void()> mOnSuccess;
  std::function<void()> mOnFailure;
  std::vector<uint8_t> mData;
};

void WriteRecord(Host_10* aHost, const std::string& aRecordName,
                 const uint8_t* aData, uint32_t aNumBytes,
                 std::function<void()>&& aOnSuccess,
                 std::function<void()>&& aOnFailure) {
  // client will be delete in WriteRecordClient::Done
  WriteRecordClient* client = new WriteRecordClient(
      std::move(aOnSuccess), std::move(aOnFailure), aData, aNumBytes);
  client->Do(aRecordName, aHost);
}

void WriteRecord(Host_10* aHost, const std::string& aRecordName,
                 const std::string& aData, std::function<void()>&& aOnSuccess,
                 std::function<void()>&& aOnFailure) {
  return WriteRecord(aHost, aRecordName, (const uint8_t*)aData.c_str(),
                     aData.size(), std::move(aOnSuccess),
                     std::move(aOnFailure));
}

class ReadRecordClient : public FileIOClient {
 public:
  explicit ReadRecordClient(
      std::function<void(bool, const uint8_t*, uint32_t)>&& aOnReadComplete)
      : mOnReadComplete(std::move(aOnReadComplete)) {}

  void OnOpenComplete(Status aStatus) override {
    auto err = aStatus;
    if (aStatus != Status::kSuccess) {
      Done(err, reinterpret_cast<const uint8_t*>(""), 0);
    } else {
      mFileIO->Read();
    }
  }

  void OnReadComplete(Status aStatus, const uint8_t* aData,
                      uint32_t aDataSize) override {
    Done(aStatus, aData, aDataSize);
  }

  void OnWriteComplete(Status aStatus) override {}

  void Do(const std::string& aName, Host_10* aHost) {
    mFileIO = aHost->CreateFileIO(this);
    mFileIO->Open(aName.c_str(), aName.size());
  }

 private:
  void Done(cdm::FileIOClient::Status aStatus, const uint8_t* aData,
            uint32_t aDataSize) {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mFileIO) {
      // will delete mFileIO inside Close.
      mFileIO->Close();
    }

    if (IO_SUCCEEDED(aStatus)) {
      mOnReadComplete(true, aData, aDataSize);
    } else {
      mOnReadComplete(false, reinterpret_cast<const uint8_t*>(""), 0);
    }

    delete this;
  }

  FileIO* mFileIO = nullptr;
  std::function<void(bool, const uint8_t*, uint32_t)> mOnReadComplete;
};

void ReadRecord(
    Host_10* aHost, const std::string& aRecordName,
    std::function<void(bool, const uint8_t*, uint32_t)>&& aOnReadComplete) {
  // client will be delete in ReadRecordClient::Done
  ReadRecordClient* client = new ReadRecordClient(std::move(aOnReadComplete));
  client->Do(aRecordName, aHost);
}

class OpenRecordClient : public FileIOClient {
 public:
  explicit OpenRecordClient(std::function<void(bool)>&& aOpenComplete)
      : mOpenComplete(std::move(aOpenComplete)) {}

  void OnOpenComplete(Status aStatus) override { Done(aStatus); }

  void OnReadComplete(Status aStatus, const uint8_t* aData,
                      uint32_t aDataSize) override {}

  void OnWriteComplete(Status aStatus) override {}

  void Do(const std::string& aName, Host_10* aHost) {
    // Initialize the FileIO.
    mFileIO = aHost->CreateFileIO(this);
    mFileIO->Open(aName.c_str(), aName.size());
  }

 private:
  void Done(cdm::FileIOClient::Status aStatus) {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mFileIO) {
      // will delete mFileIO inside Close.
      mFileIO->Close();
    }

    if (IO_SUCCEEDED(aStatus)) {
      mOpenComplete(true);
    } else {
      mOpenComplete(false);
    }

    delete this;
  }

  FileIO* mFileIO = nullptr;
  std::function<void(bool)> mOpenComplete;
};

void OpenRecord(Host_10* aHost, const std::string& aRecordName,
                std::function<void(bool)>&& aOpenComplete) {
  // client will be delete in OpenRecordClient::Done
  OpenRecordClient* client = new OpenRecordClient(std::move(aOpenComplete));
  client->Do(aRecordName, aHost);
}
