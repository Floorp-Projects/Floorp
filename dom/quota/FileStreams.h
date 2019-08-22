/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_filestreams_h__
#define mozilla_dom_quota_filestreams_h__

#include "QuotaCommon.h"

#include "nsFileStreams.h"

#include "PersistenceType.h"
#include "QuotaObject.h"

BEGIN_QUOTA_NAMESPACE

template <class FileStreamBase>
class FileQuotaStream : public FileStreamBase {
 public:
  // nsFileStreamBase override
  NS_IMETHOD
  SetEOF() override;

  NS_IMETHOD
  Close() override;

 protected:
  FileQuotaStream(PersistenceType aPersistenceType, const nsACString& aGroup,
                  const nsACString& aOrigin, Client::Type aClientType)
      : mPersistenceType(aPersistenceType),
        mGroup(aGroup),
        mOrigin(aOrigin),
        mClientType(aClientType) {}

  // nsFileStreamBase override
  virtual nsresult DoOpen() override;

  PersistenceType mPersistenceType;
  nsCString mGroup;
  nsCString mOrigin;
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
                           const nsACString& aGroup, const nsACString& aOrigin,
                           Client::Type aClientType)
      : FileQuotaStream<FileStreamBase>(aPersistenceType, aGroup, aOrigin,
                                        aClientType) {}
};

class FileInputStream : public FileQuotaStream<nsFileInputStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileInputStream,
                                       FileQuotaStream<nsFileInputStream>)

  FileInputStream(PersistenceType aPersistenceType, const nsACString& aGroup,
                  const nsACString& aOrigin, Client::Type aClientType)
      : FileQuotaStream<nsFileInputStream>(aPersistenceType, aGroup, aOrigin,
                                           aClientType) {}

 private:
  virtual ~FileInputStream() { Close(); }
};

class FileOutputStream : public FileQuotaStreamWithWrite<nsFileOutputStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(
      FileOutputStream, FileQuotaStreamWithWrite<nsFileOutputStream>);

  FileOutputStream(PersistenceType aPersistenceType, const nsACString& aGroup,
                   const nsACString& aOrigin, Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileOutputStream>(aPersistenceType, aGroup,
                                                     aOrigin, aClientType) {}

 private:
  virtual ~FileOutputStream() { Close(); }
};

class FileStream : public FileQuotaStreamWithWrite<nsFileStream> {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FileStream,
                                       FileQuotaStreamWithWrite<nsFileStream>)

  FileStream(PersistenceType aPersistenceType, const nsACString& aGroup,
             const nsACString& aOrigin, Client::Type aClientType)
      : FileQuotaStreamWithWrite<nsFileStream>(aPersistenceType, aGroup,
                                               aOrigin, aClientType) {}

 private:
  virtual ~FileStream() { Close(); }
};

already_AddRefed<FileInputStream> CreateFileInputStream(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, nsIFile* aFile,
    int32_t aIOFlags = -1, int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

already_AddRefed<FileOutputStream> CreateFileOutputStream(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, nsIFile* aFile,
    int32_t aIOFlags = -1, int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

already_AddRefed<FileStream> CreateFileStream(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, nsIFile* aFile,
    int32_t aIOFlags = -1, int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

END_QUOTA_NAMESPACE

#endif /* mozilla_dom_quota_filestreams_h__ */
