/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesWeakCallbackWrapper_h
#define mozilla_dom_PlacesWeakCallbackWrapper_h

#include "mozilla/dom/PlacesObserversBinding.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Pair.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class PlacesWeakCallbackWrapper final : public nsWrapperCache
                                      , public SupportsWeakPtr<PlacesWeakCallbackWrapper>
{
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(PlacesWeakCallbackWrapper)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PlacesWeakCallbackWrapper)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PlacesWeakCallbackWrapper)

  explicit PlacesWeakCallbackWrapper(nsISupports* aParent,
                                     PlacesEventCallback& aCallback);

  static already_AddRefed<PlacesWeakCallbackWrapper>
  Constructor(const GlobalObject& aGlobal,
              PlacesEventCallback& aCallback,
              ErrorResult& rv);

  nsISupports* GetParentObject() const;

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  friend class PlacesObservers;
  ~PlacesWeakCallbackWrapper();
  nsWeakPtr mParent;
  RefPtr<PlacesEventCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PlacesWeakCallbackWrapper_h
