/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TemporaryBlobImpl_h
#define mozilla_dom_TemporaryBlobImpl_h

#include "BaseBlobImpl.h"
#include "nsTemporaryFileInputStream.h"

namespace mozilla {
namespace dom {

class TemporaryBlobImpl final : public BaseBlobImpl
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  TemporaryBlobImpl(PRFileDesc* aFD, uint64_t aStartPos,
                   uint64_t aLength, const nsAString& aContentType);

  virtual void CreateInputStream(nsIInputStream** aStream,
                                 ErrorResult& aRv) override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;

private:
  TemporaryBlobImpl(const TemporaryBlobImpl* aOther,
                    uint64_t aStart, uint64_t aLength,
                    const nsAString& aContentType);

  ~TemporaryBlobImpl() = default;

  uint64_t mStartPos;
  RefPtr<nsTemporaryFileInputStream::FileDescOwner> mFileDescOwner;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TemporaryBlobImpl_h
