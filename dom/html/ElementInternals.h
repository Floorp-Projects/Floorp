/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInternals_h
#define mozilla_dom_ElementInternals_h

#include "js/TypeDecls.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGenericHTMLElement.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class ShadowRoot;

class ElementInternals final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ElementInternals)

  explicit ElementInternals(nsGenericHTMLElement* aTarget);

  nsISupports* GetParentObject();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  ShadowRoot* GetShadowRoot() const;

 private:
  ~ElementInternals() = default;

  // It's a target element which is a custom element.
  RefPtr<nsGenericHTMLElement> mTarget;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ElementInternals_h
