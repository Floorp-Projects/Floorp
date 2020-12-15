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
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaInfo.h"
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
                  const GroupAndOrigin& aGroupAndOrigin,
                  Client::Type aClientType)
      : mPersistenceType(aPersistenceType),
        mGroupAndOrigin(aGroupAndOrigin),
        mClientType(aClientType) {}

  // nsFileStreamBase override
  virtual nsresult DoOpen() override;

  PersistenceType mPersistenceType;
  GroupAndOrigin mGroupAndOrigin;
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
                           const GroupAndOrigin& aGroupAndOrigin,
                           Client::Type aClientType)
      : FileQuotaStream<FileStreamBase>(aPersistenceType, aGroupAndOrigin,
                                        aClientType) {}
};

class FileInputStream : public FileQuotaStream<nsFileInputStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileInputStream,
                                       FileQuotaStream<nsFileInputStream>)

  FileInputStream(PersistenceType aPersistenceType,
                  const GroupAndOrigin& aGroupAndOrigin,
                  Client::Type aClientType)
      : FileQuotaStream<nsFileInputStream>(aPersistenceType, aGroupAndOrigin,
                                           aClientType) {}

 private:
  virtual ~FileInputStream() { Close(); }
};

class FileOutputStream : public FileQuotaStreamWithWrite<nsFileOutputStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(
      FileOutputStream, FileQuotaStreamWithWrite<nsFileOutputStream>);

  FileOutputStream(PersistenceType aPersistenceType,
                   const GroupAndOrigin& aGroupAndOrigin,
                   Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileOutputStream>(
            aPersistenceType, aGroupAndOrigin, aClientType) {}

 private:
  virtual ~FileOutputStream() { Close(); }
};

class FileStream : public FileQuotaStreamWithWrite<nsFileStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileStream,
                                       FileQuotaStreamWithWrite<nsFileStream>)

  FileStream(PersistenceType aPersistenceType,
             const GroupAndOrigin& aGroupAndOrigin, Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileStream>(aPersistenceType,
                                               aGroupAndOrigin, aClientType) {}

 private:
  virtual ~FileStream() { Close(); }
};

already_AddRefed<FileInputStream> CreateFileInputStream(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

already_AddRefed<FileOutputStream> CreateFileOutputStream(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

already_AddRefed<FileStream> CreateFileStream(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags = -1,
    int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

}  // namespace mozilla::dom::quota

#endif /* mozilla_dom_quota_filestreams_h__ */
