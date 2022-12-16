/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_filestreams_h__
#define mozilla_dom_quota_filestreams_h__

// Local includes
#include "Client.h"

// Global includes
#include <cstdint>
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "nsFileStreams.h"
#include "nsISupports.h"
#include "nscore.h"

class nsIFile;

namespace mozilla {
class Runnable;
}

namespace mozilla::dom::quota {

class QuotaObject;

template <class FileStreamBase>
class FileQuotaStream : public FileStreamBase {
 public:
  // nsFileStreamBase override
  NS_IMETHOD
  SetEOF() override;

  NS_IMETHOD
  Close() override;

 protected:
  FileQuotaStream(PersistenceType aPersistenceType,
                  const OriginMetadata& aOriginMetadata,
                  Client::Type aClientType)
      : mPersistenceType(aPersistenceType),
        mOriginMetadata(aOriginMetadata),
        mClientType(aClientType),
        mDeserialized(false) {}

  FileQuotaStream()
      : mPersistenceType(PERSISTENCE_TYPE_INVALID),
        mClientType(Client::TYPE_MAX),
        mDeserialized(true) {}

  ~FileQuotaStream() { Close(); }

  // nsFileStreamBase override
  virtual nsresult DoOpen() override;

  PersistenceType mPersistenceType;
  OriginMetadata mOriginMetadata;
  Client::Type mClientType;
  RefPtr<QuotaObject> mQuotaObject;
  const bool mDeserialized;
};

template <class FileStreamBase>
class FileQuotaStreamWithWrite : public FileQuotaStream<FileStreamBase> {
 public:
  // nsFileStreamBase override
  NS_IMETHOD
  Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) override;

 protected:
  FileQuotaStreamWithWrite(PersistenceType aPersistenceType,
                           const OriginMetadata& aOriginMetadata,
                           Client::Type aClientType)
      : FileQuotaStream<FileStreamBase>(aPersistenceType, aOriginMetadata,
                                        aClientType) {}

  FileQuotaStreamWithWrite() = default;
};

class FileInputStream : public FileQuotaStream<nsFileInputStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileInputStream,
                                       FileQuotaStream<nsFileInputStream>)

  FileInputStream(PersistenceType aPersistenceType,
                  const OriginMetadata& aOriginMetadata,
                  Client::Type aClientType)
      : FileQuotaStream<nsFileInputStream>(aPersistenceType, aOriginMetadata,
                                           aClientType) {}

 private:
  virtual ~FileInputStream() { Close(); }
};

class FileOutputStream : public FileQuotaStreamWithWrite<nsFileOutputStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(
      FileOutputStream, FileQuotaStreamWithWrite<nsFileOutputStream>);

  FileOutputStream(PersistenceType aPersistenceType,
                   const OriginMetadata& aOriginMetadata,
                   Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileOutputStream>(
            aPersistenceType, aOriginMetadata, aClientType) {}

 private:
  virtual ~FileOutputStream() { Close(); }
};

// FileRandomAccessStream type is serializable, but only in a restricted
// manner. The type is only safe to serialize in the parent process and only
// when the type hasn't been previously deserialized. So the type can be
// serialized in the parent process and desrialized in a child process or it
// can be serialized in the parent process and deserialized in the parent
// process as well (non-e10s mode). The same type can never be
// serialized/deserialized more than once.
class FileRandomAccessStream
    : public FileQuotaStreamWithWrite<nsFileRandomAccessStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(
      FileRandomAccessStream,
      FileQuotaStreamWithWrite<nsFileRandomAccessStream>)

  FileRandomAccessStream(PersistenceType aPersistenceType,
                         const OriginMetadata& aOriginMetadata,
                         Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileRandomAccessStream>(
            aPersistenceType, aOriginMetadata, aClientType) {}

  FileRandomAccessStream() = default;

  // nsFileRandomAccessStream override

  // Serialize this FileRandomAccessStream. This method works only in the
  // parent process and only with streams which haven't been previously
  // deserialized.
  mozilla::ipc::RandomAccessStreamParams Serialize(
      nsIInterfaceRequestor* aCallbacks) override;

  // Deserialize this FileRandomAccessStream. This method works in both the
  // child and parent.
  bool Deserialize(mozilla::ipc::RandomAccessStreamParams& aParams) override;

 private:
  virtual ~FileRandomAccessStream() { Close(); }
};

Result<MovingNotNull<nsCOMPtr<nsIInputStream>>, nsresult> CreateFileInputStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

Result<MovingNotNull<nsCOMPtr<nsIOutputStream>>, nsresult>
CreateFileOutputStream(PersistenceType aPersistenceType,
                       const OriginMetadata& aOriginMetadata,
                       Client::Type aClientType, nsIFile* aFile,
                       int32_t aIOFlags = -1, int32_t aPerm = -1,
                       int32_t aBehaviorFlags = 0);

Result<MovingNotNull<nsCOMPtr<nsIRandomAccessStream>>, nsresult>
CreateFileRandomAccessStream(PersistenceType aPersistenceType,
                             const OriginMetadata& aOriginMetadata,
                             Client::Type aClientType, nsIFile* aFile,
                             int32_t aIOFlags = -1, int32_t aPerm = -1,
                             int32_t aBehaviorFlags = 0);

}  // namespace mozilla::dom::quota

#endif /* mozilla_dom_quota_filestreams_h__ */
