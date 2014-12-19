/*
* Copyright 2013, Mozilla Foundation and contributors
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

#ifndef GMP_STORAGE_h_
#define GMP_STORAGE_h_

#include "gmp-errors.h"
#include <stdint.h>

// Maximum size of a record, in bytes; 10 megabytes.
#define GMP_MAX_RECORD_SIZE (10 * 1024 * 1024)

// Maximum length of a record name in bytes.
#define GMP_MAX_RECORD_NAME_SIZE 2000

// Provides basic per-origin storage for CDMs. GMPRecord instances can be
// retrieved by calling GMPPlatformAPI->openstorage. Multiple GMPRecords
// with different names can be open at once, but a single record can only
// be opened by one client at a time. This interface is asynchronous, with
// results being returned via callbacks to the GMPRecordClient pointer
// provided to the GMPPlatformAPI->openstorage call, on the main thread.
//
// Lifecycle: Once opened, the GMPRecord object remains allocated until
// GMPRecord::Close() is called. If any GMPRecord function, either
// synchronously or asynchronously through a GMPRecordClient callback,
// returns an error, the GMP is responsible for calling Close() on the
// GMPRecord to delete the GMPRecord object's memory. If your GMP does not
// call Close(), the GMPRecord's memory will leak.
class GMPRecord {
public:

  // Opens the record. Calls OpenComplete() once the record is open.
  // Note: Only work when GMP is loading content from a webserver.
  // Does not work for web pages on loaded from disk.
  // Note: OpenComplete() is only called if this returns GMPNoErr.
  virtual GMPErr Open() = 0;

  // Reads the entire contents of the record, and calls
  // GMPRecordClient::ReadComplete() once the operation is complete.
  // Note: ReadComplete() is only called if this returns GMPNoErr.
  virtual GMPErr Read() = 0;

  // Writes aDataSize bytes of aData into the record, overwriting the
  // contents of the record, truncating it to aDataSize length.
  // Overwriting with 0 bytes "deletes" the record.
  // Note: WriteComplete is only called if this returns GMPNoErr.
  virtual GMPErr Write(const uint8_t* aData, uint32_t aDataSize) = 0;

  // Closes a record, deletes the GMPRecord object. The GMPRecord object
  // must not be used after this is called, request a new one with
  // GMPPlatformAPI->openstorage to re-open this record. Cancels all
  // callbacks.
  virtual GMPErr Close() = 0;

  virtual ~GMPRecord() {}
};

// Callback object that receives the results of GMPRecord calls. Callbacks
// run asynchronously to the GMPRecord call, on the main thread.
class GMPRecordClient {
 public:

  // Response to a GMPRecord::Open() call with the open |status|.
  // aStatus values:
  // - GMPNoErr - Record opened successfully. Record may be empty.
  // - GMPRecordInUse - This record is in use by another client.
  // - GMPGenericErr - Unspecified error.
  // If aStatus is not GMPNoErr, the GMPRecord is unusable, and you must
  // call Close() on the GMPRecord to dispose of it.
  virtual void OpenComplete(GMPErr aStatus) = 0;

  // Response to a GMPRecord::Read() call, where aData is the record contents,
  // of length aDataSize.
  // aData is only valid for the duration of the call to ReadComplete.
  // Copy it if you want to hang onto it!
  // aStatus values:
  // - GMPNoErr - Record contents read successfully, aDataSize 0 means record
  //   is empty.
  // - GMPRecordInUse - There are other operations or clients in use on
  //   this record.
  // - GMPGenericErr - Unspecified error.
  // If aStatus is not GMPNoErr, the GMPRecord is unusable, and you must
  // call Close() on the GMPRecord to dispose of it.
  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) = 0;

  // Response to a GMPRecord::Write() call.
  // - GMPNoErr - File contents written successfully.
  // - GMPRecordInUse - There are other operations or clients in use on
  //   this record.
  // - GMPGenericErr - Unspecified error.
  // If aStatus is not GMPNoErr, the GMPRecord is unusable, and you must
  // call Close() on the GMPRecord to dispose of it.
  virtual void WriteComplete(GMPErr aStatus) = 0;

  virtual ~GMPRecordClient() {}
};

// Iterates over the records that are available. Note: this list maintains
// a snapshot of the records that were present when the iterator was created.
// Create by calling the GMPCreateRecordIteratorPtr function on the
// GMPPlatformAPI struct.
// Iteration is in alphabetical order.
class GMPRecordIterator {
public:
  // Retrieve the name for the current record.
  // aOutName is null terminated at character  at index (*aOutNameLength).
  // Returns GMPNoErr if successful, or GMPEndOfEnumeration if iteration has
  // reached the end.
  virtual GMPErr GetName(const char ** aOutName, uint32_t * aOutNameLength) = 0;

  // Advance iteration to the next record.
  // Returns GMPNoErr if successful, or GMPEndOfEnumeration if iteration has
  // reached the end.
  virtual GMPErr NextRecord() = 0;

  // Signals to the GMP host that the GMP is finished with the
  // GMPRecordIterator. GMPs must call this to release memory held by
  // the GMPRecordIterator. Do not access the GMPRecordIterator pointer
  // after calling this!
  // Memory retrieved by GetName is *not* valid after calling Close()!
  virtual void Close() = 0;

  virtual ~GMPRecordIterator() {}
};

#endif // GMP_STORAGE_h_
