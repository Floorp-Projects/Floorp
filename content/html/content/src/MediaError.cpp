/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaError.h"
#include "nsDOMClassInfoID.h"

DOMCI_DATA(MediaError, mozilla::dom::MediaError)

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF(MediaError)
NS_IMPL_RELEASE(MediaError)

NS_INTERFACE_MAP_BEGIN(MediaError)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MediaError)
NS_INTERFACE_MAP_END

MediaError::MediaError(uint16_t aCode)
: mCode(aCode)
{
}

NS_IMETHODIMP MediaError::GetCode(uint16_t* aCode)
{
  if (aCode)
    *aCode = mCode;

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
