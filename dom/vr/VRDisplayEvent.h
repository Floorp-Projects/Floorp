/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VRDisplayEvent_h_
#define mozilla_dom_VRDisplayEvent_h_

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/VRDisplayEventBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Event.h"

#include "gfxVR.h"

struct JSContext;

namespace mozilla {
namespace gfx {
class VRDisplay;
} // namespace gfx

namespace dom {

class VRDisplayEvent final : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(VRDisplayEvent, Event)

  VRDisplay* Display();
  Nullable<VRDisplayEventReason> GetReason() const;

protected:
  virtual ~VRDisplayEvent();
  explicit VRDisplayEvent(mozilla::dom::EventTarget* aOwner);
  VRDisplayEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 InternalClipboardEvent* aEvent);

  Maybe<VRDisplayEventReason> mReason;
  RefPtr<VRDisplay> mDisplay;

public:

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VRDisplayEvent> Constructor(mozilla::dom::EventTarget* aOwner, const nsAString& aType, const VRDisplayEventInit& aEventInitDict);

  static already_AddRefed<VRDisplayEvent> Constructor(const GlobalObject& aGlobal, const nsAString& aType, const VRDisplayEventInit& aEventInitDict, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif
