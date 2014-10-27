/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamError.h"
#include "mozilla/dom/MediaStreamErrorBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

MediaStreamError::MediaStreamError(
    nsPIDOMWindow* aParent,
    const nsAString& aName,
    const nsAString& aMessage,
    const nsAString& aConstraintName)
  : mParent(aParent)
  , mName(aName)
  , mMessage(aMessage)
  , mConstraintName(aConstraintName) {}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaStreamError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamError)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
MediaStreamError::WrapObject(JSContext* aCx)
{
  return MediaStreamErrorBinding::Wrap(aCx, this);
}

void
MediaStreamError::GetName(nsAString& aName) const
{
  aName = mName;
}

void
MediaStreamError::GetMessage(nsAString& aMessage) const
{
  aMessage = mMessage;
}

void
MediaStreamError::GetConstraintName(nsAString& aConstraintName) const
{
  aConstraintName = mConstraintName;
}

} // namespace dom
} // namespace mozilla
