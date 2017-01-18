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

#include "ClearKeyStorage.h"

#include "ClearKeyUtils.h"

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"

#include <assert.h>
#include "ArrayUtils.h"
#include <vector>

using namespace cdm;
using namespace std;

class WriteRecordClient : public FileIOClient
{
public:
  /*
   * This function will take the memory ownership of the parameters and
   * delete them when done.
   */
  static void Write(Host_8* aHost,
                    string& aRecordName,
                    const vector<uint8_t>& aData,
                    function<void()>&& aOnSuccess,
                    function<void()>&& aOnFailure)
{
    WriteRecordClient* client = new WriteRecordClient(aData,
                                                      move(aOnSuccess),
                                                      move(aOnFailure));
    client->Do(aRecordName, aHost);
  }

  void OnOpenComplete(Status aStatus) override
  {
    // If we hit an error, fail.
    if (aStatus != Status::kSuccess) {
      Done(aStatus);
    } else if (mFileIO) { // Otherwise, write our data to the file.
      mFileIO->Write(&mData[0], mData.size());
    }
  }

  void OnReadComplete(Status aStatus,
                      const uint8_t* aData,
                      uint32_t aDataSize) override
  {
    // This function should never be called, we only ever write data with this
    // client.
    assert(false);
  }

  void OnWriteComplete(Status aStatus) override
  {
    Done(aStatus);
  }

private:
  explicit WriteRecordClient(const vector<uint8_t>& aData,
                             function<void()>&& aOnSuccess,
                             function<void()>&& aOnFailure)
    : mFileIO(nullptr)
    , mOnSuccess(move(aOnSuccess))
    , mOnFailure(move(aOnFailure))
    , mData(aData) {}

  void Do(const string& aName, Host_8* aHost)
  {
    // Initialize the FileIO.
    mFileIO = aHost->CreateFileIO(this);
    mFileIO->Open(aName.c_str(), aName.size());
  }

  void Done(cdm::FileIOClient::Status aStatus)
  {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mFileIO) {
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

  function<void()> mOnSuccess;
  function<void()> mOnFailure;

  const vector<uint8_t> mData;
};

void
WriteData(Host_8* aHost,
          string& aRecordName,
          const vector<uint8_t>& aData,
          function<void()>&& aOnSuccess,
          function<void()>&& aOnFailure)
{
  WriteRecordClient::Write(aHost,
                           aRecordName,
                           aData,
                           move(aOnSuccess),
                           move(aOnFailure));
}

class ReadRecordClient : public FileIOClient
{
public:
  /*
   * This function will take the memory ownership of the parameters and
   * delete them when done.
   */
  static void Read(Host_8* aHost,
                   string& aRecordName,
                   function<void(const uint8_t*, uint32_t)>&& aOnSuccess,
                   function<void()>&& aOnFailure)
  {

    (new ReadRecordClient(move(aOnSuccess), move(aOnFailure)))->
      Do(aRecordName, aHost);
  }

  void OnOpenComplete(Status aStatus) override
  {
    auto err = aStatus;
    if (aStatus != Status::kSuccess) {
      Done(err, nullptr, 0);
    } else {
      mFileIO->Read();
    }
  }

  void OnReadComplete(Status aStatus,
                      const uint8_t* aData,
                      uint32_t aDataSize) override
  {
    Done(aStatus, aData, aDataSize);
  }

  void OnWriteComplete(Status aStatus) override
  {
    // We should never reach here, this client only ever reads data.
    assert(false);
  }

private:
  explicit ReadRecordClient(function<void(const uint8_t*, uint32_t)>&& aOnSuccess,
                            function<void()>&& aOnFailure)
    : mFileIO(nullptr)
    , mOnSuccess(move(aOnSuccess))
    , mOnFailure(move(aOnFailure))
  {}

  void Do(const string& aName, Host_8* aHost)
  {
    mFileIO = aHost->CreateFileIO(this);
    mFileIO->Open(aName.c_str(), aName.size());
  }

  void Done(cdm::FileIOClient::Status aStatus,
            const uint8_t* aData,
            uint32_t aDataSize)
  {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mFileIO) {
      mFileIO->Close();
    }

    if (IO_SUCCEEDED(aStatus)) {
      mOnSuccess(aData, aDataSize);
    } else {
      mOnFailure();
    }

    delete this;
  }

  FileIO* mFileIO = nullptr;

  function<void(const uint8_t*, uint32_t)> mOnSuccess;
  function<void()> mOnFailure;
};

void
ReadData(Host_8* mHost,
         string& aRecordName,
         function<void(const uint8_t*, uint32_t)>&& aOnSuccess,
         function<void()>&& aOnFailure)
{
  ReadRecordClient::Read(mHost,
                         aRecordName,
                         move(aOnSuccess),
                         move(aOnFailure));
}
