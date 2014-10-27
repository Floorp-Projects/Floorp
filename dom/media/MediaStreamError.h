/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaStreamError_h
#define mozilla_dom_MediaStreamError_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"
#include "nsPIDOMWindow.h"
#include "nsRefPtr.h"

#if defined(XP_WIN) && defined(GetMessage)
#undef GetMessage
#endif

namespace mozilla {
namespace dom {

class MediaStreamError : public nsISupports, public nsWrapperCache
{
public:
  MediaStreamError(nsPIDOMWindow* aParent,
                   const nsAString& aName,
                   const nsAString& aMessage,
                   const nsAString& aConstraintName);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaStreamError)

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsPIDOMWindow* GetParentObject() const
  {
    return mParent;
  }
  void GetName(nsAString& aName) const;
  void GetMessage(nsAString& aMessage) const;
  void GetConstraintName(nsAString& aConstraintName) const;

private:
  virtual ~MediaStreamError() {}

  nsRefPtr<nsPIDOMWindow> mParent;
  const nsString mName;
  const nsString mMessage;
  const nsString mConstraintName;
};

} // namespace dom
} // namespace mozilla

#endif
