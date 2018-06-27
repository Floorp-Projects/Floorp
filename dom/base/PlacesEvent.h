/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesEvent_h
#define mozilla_dom_PlacesEvent_h

#include "mozilla/dom/PlacesEventBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class PlacesEvent : public nsWrapperCache
{
public:
  explicit PlacesEvent(PlacesEventType aType)
    : mType(aType)
  {}

  static already_AddRefed<PlacesEvent>
  Constructor(const GlobalObject& aGlobal,
              PlacesEventType aType,
              ErrorResult& aRv);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PlacesEvent)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PlacesEvent)

  nsISupports* GetParentObject() const;

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  PlacesEventType Type() const { return mType; }

  virtual const PlacesVisit* AsPlacesVisit() const { return nullptr; }
protected:
  virtual ~PlacesEvent() = default;
  PlacesEventType mType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PlacesEvent_h
