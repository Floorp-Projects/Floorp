/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EmptyBlobImpl_h
#define mozilla_dom_EmptyBlobImpl_h

#include "BaseBlobImpl.h"

namespace mozilla {
namespace dom {

class EmptyBlobImpl final : public BaseBlobImpl
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit EmptyBlobImpl(const nsAString& aContentType)
    : BaseBlobImpl(aContentType, 0 /* aLength */)
  {
    mImmutable = true;
  }

  EmptyBlobImpl(const nsAString& aName,
                const nsAString& aContentType,
                int64_t aLastModifiedDate)
    : BaseBlobImpl(aName, aContentType, 0, aLastModifiedDate)
  {
    mImmutable = true;
  }

  virtual void GetInternalStream(nsIInputStream** aStream,
                                 ErrorResult& aRv) override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;

  virtual bool IsMemoryFile() const override
  {
    return true;
  }

private:
  ~EmptyBlobImpl() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EmptyBlobImpl_h
