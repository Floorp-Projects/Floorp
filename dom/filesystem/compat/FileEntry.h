/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileEntry_h
#define mozilla_dom_FileEntry_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class FileEntry final : public Entry
{
public:
  explicit FileEntry(nsIGlobalObject* aGlobalObject);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  CreateWriter(VoidCallback& aSuccessCallback,
               const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
               ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  void
  File(BlobCallback& aSuccessCallback,
       const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
       ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

private:
  ~FileEntry();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileEntry_h
