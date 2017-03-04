/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_IntlUtils_h
#define mozilla_dom_IntlUtils_h

#include "mozilla/dom/IntlUtilsBinding.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class IntlUtils final : public nsISupports
                      , public nsWrapperCache
{
public:
  explicit IntlUtils(nsPIDOMWindowInner* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IntlUtils)

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetDisplayNames(const Sequence<nsString>& aLocales,
                  const mozilla::dom::DisplayNameOptions& aOptions,
                  mozilla::dom::DisplayNameResult& aResult,
                  mozilla::ErrorResult& aError);

private:
  ~IntlUtils();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

} // namespace dom
} // namespace mozilla
#endif
