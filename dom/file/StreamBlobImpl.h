/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StreamBlobImpl_h
#define mozilla_dom_StreamBlobImpl_h

#include "BaseBlobImpl.h"
#include "nsIMemoryReporter.h"

namespace mozilla {
namespace dom {

class StreamBlobImpl final : public BaseBlobImpl
                           , public nsIMemoryReporter
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMEMORYREPORTER

  static already_AddRefed<StreamBlobImpl>
  Create(nsIInputStream* aInputStream,
         const nsAString& aContentType,
         uint64_t aLength);

  static already_AddRefed<StreamBlobImpl>
  Create(nsIInputStream* aInputStream,
         const nsAString& aName,
         const nsAString& aContentType,
         int64_t aLastModifiedDate,
         uint64_t aLength);

  virtual void GetInternalStream(nsIInputStream** aStream,
                                 ErrorResult& aRv) override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;

  virtual bool IsMemoryFile() const override
  {
    return true;
  }

  void SetFullPath(const nsAString& aFullPath)
  {
    mFullPath = aFullPath;
  }

  void GetMozFullPathInternal(nsAString& aFullPath,
                              ErrorResult& aRv) const override
  {
    aFullPath = mFullPath;
  }

  void SetIsDirectory(bool aIsDirectory)
  {
    mIsDirectory = aIsDirectory;
  }

  bool IsDirectory() const override
  {
    return mIsDirectory;
  }

private:
  StreamBlobImpl(nsIInputStream* aInputStream,
                 const nsAString& aContentType,
                 uint64_t aLength);

  StreamBlobImpl(nsIInputStream* aInputStream,
                 const nsAString& aName,
                 const nsAString& aContentType,
                 int64_t aLastModifiedDate,
                 uint64_t aLength);

  StreamBlobImpl(StreamBlobImpl* aOther,
                 const nsAString& aContentType,
                 uint64_t aStart,
                 uint64_t aLength);

  ~StreamBlobImpl();

  void MaybeRegisterMemoryReporter();

  nsCOMPtr<nsIInputStream> mInputStream;

  nsString mFullPath;
  bool mIsDirectory;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StreamBlobImpl_h
