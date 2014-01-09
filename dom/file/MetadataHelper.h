/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_metadatahelper_h__
#define mozilla_dom_file_metadatahelper_h__

#include "mozilla/Attributes.h"
#include "FileCommon.h"

#include "nsIFileStreams.h"

#include "DictionaryHelpers.h"

#include "AsyncHelper.h"
#include "FileHelper.h"

class nsIFileStream;

BEGIN_FILE_NAMESPACE

class MetadataHelper;

class MetadataParameters
{
  friend class MetadataHelper;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MetadataParameters)

  nsresult
  Init(JSContext* aCx, JS::Handle<JS::Value> aVal)
  {
    return mConfig.Init(aCx, aVal.address());
  }

  void
  Init(bool aRequestSize, bool aRequestLastModified)
  {
    mConfig.size = aRequestSize;
    mConfig.lastModified = aRequestLastModified;
  }

  bool
  IsConfigured() const
  {
    return mConfig.size || mConfig.lastModified;
  }

  bool
  SizeRequested() const
  {
    return mConfig.size;
  }

  bool
  LastModifiedRequested() const
  {
    return mConfig.lastModified;
  }

  uint64_t
  Size() const
  {
    return mSize;
  }

  int64_t
  LastModified() const
  {
    return mLastModified;
  }

private:
  mozilla::idl::DOMFileMetadataParameters mConfig;

  uint64_t mSize;
  int64_t mLastModified;
};

class MetadataHelper : public FileHelper
{
public:
  MetadataHelper(LockedFile* aLockedFile,
                 FileRequest* aFileRequest,
                 MetadataParameters* aParams)
  : FileHelper(aLockedFile, aFileRequest),
    mParams(aParams)
  { }

  nsresult
  DoAsyncRun(nsISupports* aStream) MOZ_OVERRIDE;

  nsresult
  GetSuccessResult(JSContext* aCx, JS::Value* aVal) MOZ_OVERRIDE;

protected:
  class AsyncMetadataGetter : public AsyncHelper
  {
  public:
    AsyncMetadataGetter(nsISupports* aStream, MetadataParameters* aParams,
                        bool aReadWrite)
    : AsyncHelper(aStream),
      mParams(aParams), mReadWrite(aReadWrite)
    { }

  protected:
    nsresult
    DoStreamWork(nsISupports* aStream) MOZ_OVERRIDE;

  private:
    nsRefPtr<MetadataParameters> mParams;
    bool mReadWrite;
  };

  nsRefPtr<MetadataParameters> mParams;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_metadatahelper_h__
