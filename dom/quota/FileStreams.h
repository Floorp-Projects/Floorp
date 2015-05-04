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
class FileQuotaStream : public FileStreamBase
{
public:
  // nsFileStreamBase override
  NS_IMETHOD
  SetEOF() override;

  NS_IMETHOD
  Close() override;

protected:
  FileQuotaStream(PersistenceType aPersistenceType, const nsACString& aGroup,
                  const nsACString& aOrigin)
  : mPersistenceType(aPersistenceType), mGroup(aGroup), mOrigin(aOrigin)
  { }

  // nsFileStreamBase override
  virtual nsresult
  DoOpen() override;

  PersistenceType mPersistenceType;
  nsCString mGroup;
  nsCString mOrigin;
  nsRefPtr<QuotaObject> mQuotaObject;
};

template <class FileStreamBase>
class FileQuotaStreamWithWrite : public FileQuotaStream<FileStreamBase>
{
public:
  // nsFileStreamBase override
  NS_IMETHOD
  Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) override;

protected:
  FileQuotaStreamWithWrite(PersistenceType aPersistenceType,
                           const nsACString& aGroup, const nsACString& aOrigin)
  : FileQuotaStream<FileStreamBase>(aPersistenceType, aGroup, aOrigin)
  { }
};

class FileInputStream : public FileQuotaStream<nsFileInputStream>
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<FileInputStream>
  Create(PersistenceType aPersistenceType, const nsACString& aGroup,
         const nsACString& aOrigin, nsIFile* aFile, int32_t aIOFlags = -1,
         int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

private:
  FileInputStream(PersistenceType aPersistenceType, const nsACString& aGroup,
                  const nsACString& aOrigin)
  : FileQuotaStream<nsFileInputStream>(aPersistenceType, aGroup, aOrigin)
  { }

  virtual ~FileInputStream() {
    Close();
  }
};

class FileOutputStream : public FileQuotaStreamWithWrite<nsFileOutputStream>
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<FileOutputStream>
  Create(PersistenceType aPersistenceType, const nsACString& aGroup,
         const nsACString& aOrigin, nsIFile* aFile, int32_t aIOFlags = -1,
         int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

private:
  FileOutputStream(PersistenceType aPersistenceType, const nsACString& aGroup,
                   const nsACString& aOrigin)
  : FileQuotaStreamWithWrite<nsFileOutputStream>(aPersistenceType, aGroup,
                                                 aOrigin)
  { }

  virtual ~FileOutputStream() {
    Close();
  }
};

class FileStream : public FileQuotaStreamWithWrite<nsFileStream>
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<FileStream>
  Create(PersistenceType aPersistenceType, const nsACString& aGroup,
         const nsACString& aOrigin, nsIFile* aFile, int32_t aIOFlags = -1,
         int32_t aPerm = -1, int32_t aBehaviorFlags = 0);

private:
  FileStream(PersistenceType aPersistenceType, const nsACString& aGroup,
             const nsACString& aOrigin)
  : FileQuotaStreamWithWrite<nsFileStream>(aPersistenceType, aGroup, aOrigin)
  { }

  virtual ~FileStream() {
    Close();
  }
};

END_QUOTA_NAMESPACE

#endif /* mozilla_dom_quota_filestreams_h__ */
