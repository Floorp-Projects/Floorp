/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_FluentBundle_h
#define mozilla_intl_l10n_FluentBundle_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/FluentBinding.h"
#include "mozilla/intl/FluentBindings.h"

namespace mozilla {

namespace dom {
struct FluentMessage;
}  // namespace dom

namespace intl {

class FluentPattern : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(FluentPattern)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(FluentPattern)

  FluentPattern(nsISupports* aParent, const nsACString& aId);
  FluentPattern(nsISupports* aParent, const nsACString& aId,
                const nsACString& aAttrName);
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() const { return mParent; }

  nsCString mId;
  nsCString mAttrName;

 protected:
  virtual ~FluentPattern();

  nsCOMPtr<nsISupports> mParent;
};

}  // namespace intl
}  // namespace mozilla

#endif
