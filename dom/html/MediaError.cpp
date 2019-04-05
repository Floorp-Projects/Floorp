/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaError.h"

#include <string>
#include <unordered_set>

#include "mozilla/dom/MediaErrorBinding.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "jsapi.h"
#include "js/Warnings.h"  // JS::WarnASCII

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaError)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MediaError::MediaError(HTMLMediaElement* aParent, uint16_t aCode,
                       const nsACString& aMessage)
    : mParent(aParent), mCode(aCode), mMessage(aMessage) {}

void MediaError::GetMessage(nsAString& aResult) const {
  // When fingerprinting resistance is enabled, only messages in this list
  // can be returned to content script.
  static const std::unordered_set<std::string> whitelist = {
      "404: Not Found"
      // TODO
  };

  bool shouldBlank = (whitelist.find(mMessage.get()) == whitelist.end());

  if (shouldBlank) {
    // Print a warning message to JavaScript console to alert developers of
    // a non-whitelisted error message.
    nsAutoCString message =
        NS_LITERAL_CSTRING(
            "This error message will be blank when "
            "privacy.resistFingerprinting = true."
            "  If it is really necessary, please add it to the whitelist in"
            " MediaError::GetMessage: ") +
        mMessage;
    Document* ownerDoc = mParent->OwnerDoc();
    AutoJSAPI api;
    if (api.Init(ownerDoc->GetScopeObject())) {
      // We prefer this API because it can also print to our debug log and
      // try server's log viewer.
      JS::WarnASCII(api.cx(), "%s", message.get());
    } else {
      // If failed to use JS::WarnASCII, fall back to
      // nsContentUtils::ReportToConsoleNonLocalized, which can only print to
      // JavaScript console.
      nsContentUtils::ReportToConsoleNonLocalized(
          NS_ConvertASCIItoUTF16(message), nsIScriptError::warningFlag,
          NS_LITERAL_CSTRING("MediaError"), ownerDoc);
    }
  }

  if (!nsContentUtils::IsCallerChrome() &&
      nsContentUtils::ShouldResistFingerprinting(mParent->OwnerDoc()) &&
      shouldBlank) {
    aResult.Truncate();
    return;
  }

  CopyUTF8toUTF16(mMessage, aResult);
}

JSObject* MediaError::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return MediaError_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
