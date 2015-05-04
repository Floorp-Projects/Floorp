/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaError.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/dom/MediaErrorBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaError)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaError)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMediaError)
NS_INTERFACE_MAP_END

MediaError::MediaError(HTMLMediaElement* aParent, uint16_t aCode)
  : mParent(aParent)
  , mCode(aCode)
{
}

NS_IMETHODIMP MediaError::GetCode(uint16_t* aCode)
{
  if (aCode)
    *aCode = Code();

  return NS_OK;
}

JSObject*
MediaError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaErrorBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
