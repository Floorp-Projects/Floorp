/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaError_h
#define mozilla_dom_MediaError_h

#include "mozilla/dom/HTMLMediaElement.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "mozilla/Attributes.h"

namespace mozilla::dom {

class MediaError final : public nsISupports, public nsWrapperCache {
  ~MediaError() = default;

 public:
  MediaError(HTMLMediaElement* aParent, uint16_t aCode,
             const nsACString& aMessage = nsCString());

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MediaError)

  HTMLMediaElement* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  uint16_t Code() const { return mCode; }

  void GetMessage(nsAString& aResult) const;

 private:
  RefPtr<HTMLMediaElement> mParent;

  // Error code
  const uint16_t mCode;
  // Error details;
  const nsCString mMessage;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MediaError_h
