/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamError.h"
#include "mozilla/dom/MediaStreamErrorBinding.h"
#include "nsContentUtils.h"

namespace mozilla {

BaseMediaMgrError::BaseMediaMgrError(const nsAString& aName,
                                     const nsAString& aMessage,
                                     const nsAString& aConstraint)
  : mName(aName)
  , mMessage(aMessage)
  , mConstraint(aConstraint)
{
  if (mMessage.IsEmpty()) {
    if (mName.EqualsLiteral("NotFoundError")) {
      mMessage.AssignLiteral("The object can not be found here.");
    } else if (mName.EqualsLiteral("NotAllowedError")) {
      mMessage.AssignLiteral("The request is not allowed by the user agent "
                             "or the platform in the current context.");
    } else if (mName.EqualsLiteral("SecurityError")) {
      mMessage.AssignLiteral("The operation is insecure.");
    } else if (mName.EqualsLiteral("SourceUnavailableError")) {
      mMessage.AssignLiteral("The source of the MediaStream could not be "
          "accessed due to a hardware error (e.g. lock from another process).");
    } else if (mName.EqualsLiteral("InternalError")) {
      mMessage.AssignLiteral("Internal error.");
    } else if (mName.EqualsLiteral("NotSupportedError")) {
      mMessage.AssignLiteral("The operation is not supported.");
    } else if (mName.EqualsLiteral("OverconstrainedError")) {
      mMessage.AssignLiteral("Constraints could be not satisfied.");
    }
  }
}


NS_IMPL_ISUPPORTS0(MediaMgrError)

namespace dom {

MediaStreamError::MediaStreamError(
    nsPIDOMWindowInner* aParent,
    const nsAString& aName,
    const nsAString& aMessage,
    const nsAString& aConstraint)
  : BaseMediaMgrError(aName, aMessage, aConstraint)
  , mParent(aParent) {}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaStreamError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamError)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(MediaStreamError)
NS_INTERFACE_MAP_END

JSObject*
MediaStreamError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaStreamErrorBinding::Wrap(aCx, this, aGivenProto);
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
MediaStreamError::GetConstraint(nsAString& aConstraint) const
{
  aConstraint = mConstraint;
}

} // namespace dom
} // namespace mozilla
