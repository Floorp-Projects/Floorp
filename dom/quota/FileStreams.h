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
        mClientType(aClientType) {}

  // nsFileStreamBase override
  virtual nsresult DoOpen() override;

  PersistenceType mPersistenceType;
  OriginMetadata mOriginMetadata;
  Client::Type mClientType;
  RefPtr<QuotaObject> mQuotaObject;
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

class FileStream : public FileQuotaStreamWithWrite<nsFileStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileStream,
                                       FileQuotaStreamWithWrite<nsFileStream>)

  FileStream(PersistenceType aPersistenceType,
             const OriginMetadata& aOriginMetadata, Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileStream>(aPersistenceType,
                                               aOriginMetadata, aClientType) {}

 private:
  virtual ~FileStream() { Close(); }
};

Result<NotNull<RefPtr<FileInputStream>>, nsresult> CreateFileInputStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

Result<NotNull<RefPtr<FileOutputStream>>, nsresult> CreateFileOutputStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

Result<NotNull<RefPtr<FileStream>>, nsresult> CreateFileStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

}  // namespace mozilla::dom::quota

#endif /* mozilla_dom_quota_filestreams_h__ */
