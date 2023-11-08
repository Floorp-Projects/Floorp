/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CustomStateSet_h
#define mozilla_dom_CustomStateSet_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

namespace mozilla::dom {

class HTMLElement;
class GlobalObject;

class CustomStateSet final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(CustomStateSet)

  explicit CustomStateSet(HTMLElement* aTarget);

  // WebIDL interface
  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<CustomStateSet>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  void InvalidateStyleFromCustomStateSetChange() const;

  MOZ_CAN_RUN_SCRIPT void Clear(ErrorResult& aRv);

  /**
   * @brief Removes a given string from the state set.
   */
  MOZ_CAN_RUN_SCRIPT bool Delete(const nsAString& aState, ErrorResult& aRv);

  /**
   * @brief Adds a string to this state set.
   */
  MOZ_CAN_RUN_SCRIPT void Add(const nsAString& aState, ErrorResult& aRv);

 private:
  virtual ~CustomStateSet() = default;

  RefPtr<HTMLElement> mTarget;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CustomStateSet_h
