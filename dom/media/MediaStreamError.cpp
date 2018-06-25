/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamError.h"
#include "mozilla/dom/MediaStreamErrorBinding.h"
#include "nsContentUtils.h"

namespace mozilla {

BaseMediaMgrError::BaseMediaMgrError(Name aName,
                                     const nsAString& aMessage,
                                     const nsAString& aConstraint)
  : mMessage(aMessage)
  , mConstraint(aConstraint)
  , mName(aName)
{

#define MAP_MEDIAERR(name, msg) { Name::name, #name, msg }

  static struct {
    Name mName;
    const char* mNameString;
    const char* mMessage;
  } map[] = {
    MAP_MEDIAERR(AbortError, "The operation was aborted."),
    MAP_MEDIAERR(InvalidStateError, "The object is in an invalid state."),
    MAP_MEDIAERR(NotAllowedError, "The request is not allowed by the user agent "
                                  "or the platform in the current context."),
    MAP_MEDIAERR(NotFoundError, "The object can not be found here."),
    MAP_MEDIAERR(NotReadableError, "The I/O read operation failed."),
    MAP_MEDIAERR(NotSupportedError, "The operation is not supported."),
    MAP_MEDIAERR(OverconstrainedError, "Constraints could be not satisfied."),
    MAP_MEDIAERR(SecurityError, "The operation is insecure."),
    MAP_MEDIAERR(TypeError, ""),
  };
  for (auto& entry : map) {
    if (entry.mName == mName) {
      mNameString.AssignASCII(entry.mNameString);
      if (mMessage.IsEmpty()) {
        mMessage.AssignASCII(entry.mMessage);
      }
      return;
    }
  }
  MOZ_ASSERT_UNREACHABLE("Unknown error type");
}

NS_IMPL_ISUPPORTS0(MediaMgrError)

namespace dom {

MediaStreamError::MediaStreamError(
    nsPIDOMWindowInner* aParent,
    Name aName,
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
  return MediaStreamError_Binding::Wrap(aCx, this, aGivenProto);
}

void
MediaStreamError::GetName(nsAString& aName) const
{
  aName = mNameString;
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
