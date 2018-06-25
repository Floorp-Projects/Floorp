/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRDisplayEvent.h"
#include "js/RootingAPI.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PrimitiveConversions.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(VRDisplayEvent)

NS_IMPL_ADDREF_INHERITED(VRDisplayEvent, Event)
NS_IMPL_RELEASE_INHERITED(VRDisplayEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(VRDisplayEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(VRDisplayEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(VRDisplayEvent, Event)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VRDisplayEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

VRDisplayEvent::VRDisplayEvent(mozilla::dom::EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
}

VRDisplay*
VRDisplayEvent::Display()
{
  return mDisplay;
}

JSObject*
VRDisplayEvent::WrapObjectInternal(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return VRDisplayEvent_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<VRDisplayEvent>
VRDisplayEvent::Constructor(mozilla::dom::EventTarget* aOwner,
                            const nsAString& aType,
                            const VRDisplayEventInit& aEventInitDict)
{
  RefPtr<VRDisplayEvent> e = new VRDisplayEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  if (aEventInitDict.mReason.WasPassed()) {
    e->mReason = Some(aEventInitDict.mReason.Value());
  }
  e->mDisplay = aEventInitDict.mDisplay;
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

already_AddRefed<VRDisplayEvent>
VRDisplayEvent::Constructor(const GlobalObject& aGlobal, const nsAString& aType,
                            const VRDisplayEventInit& aEventInitDict,
                            ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

Nullable<VRDisplayEventReason>
VRDisplayEvent::GetReason() const
{
  if (mReason.isSome()) {
    return mReason.value();
  }

  return nullptr;
}


} // namespace dom
} // namespace mozilla
