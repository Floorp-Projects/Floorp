/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamError.h"
#include "mozilla/dom/MediaStreamErrorBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"

namespace mozilla {

BaseMediaMgrError::BaseMediaMgrError(Name aName, const nsACString& aMessage,
                                     const nsAString& aConstraint)
    : mMessage(aMessage), mConstraint(aConstraint), mName(aName) {
#define MAP_MEDIAERR(name, msg) \
  { Name::name, #name, msg }

  static struct {
    Name mName;
    const char* mNameString;
    const char* mMessage;
  } map[] = {
      MAP_MEDIAERR(AbortError, "The operation was aborted."),
      MAP_MEDIAERR(InvalidStateError, "The object is in an invalid state."),
      MAP_MEDIAERR(NotAllowedError,
                   "The request is not allowed by the user agent "
                   "or the platform in the current context."),
      MAP_MEDIAERR(NotFoundError, "The object can not be found here."),
      MAP_MEDIAERR(NotReadableError, "The I/O read operation failed."),
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

void MediaMgrError::Reject(dom::Promise* aPromise) const {
  switch (mName) {
    case Name::AbortError:
      aPromise->MaybeRejectWithAbortError(mMessage);
      return;
    case Name::InvalidStateError:
      aPromise->MaybeRejectWithInvalidStateError(mMessage);
      return;
    case Name::NotAllowedError:
      aPromise->MaybeRejectWithNotAllowedError(mMessage);
      return;
    case Name::NotFoundError:
      aPromise->MaybeRejectWithNotFoundError(mMessage);
      return;
    case Name::NotReadableError:
      aPromise->MaybeRejectWithNotReadableError(mMessage);
      return;
    case Name::OverconstrainedError: {
      // TODO: Add OverconstrainedError type.
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1453013
      nsCOMPtr<nsPIDOMWindowInner> window =
          do_QueryInterface(aPromise->GetGlobalObject());
      aPromise->MaybeReject(MakeRefPtr<dom::MediaStreamError>(window, *this));
      return;
    }
    case Name::SecurityError:
      aPromise->MaybeRejectWithSecurityError(mMessage);
      return;
    case Name::TypeError:
      aPromise->MaybeRejectWithTypeError(mMessage);
      return;
      // -Wswitch ensures all cases are covered so don't add default:.
  }
}

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaStreamError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamError)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(MediaStreamError)
NS_INTERFACE_MAP_END

JSObject* MediaStreamError::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return MediaStreamError_Binding::Wrap(aCx, this, aGivenProto);
}

void MediaStreamError::GetName(nsAString& aName) const { aName = mNameString; }

void MediaStreamError::GetMessage(nsAString& aMessage) const {
  CopyUTF8toUTF16(mMessage, aMessage);
}

void MediaStreamError::GetConstraint(nsAString& aConstraint) const {
  aConstraint = mConstraint;
}

}  // namespace dom
}  // namespace mozilla
