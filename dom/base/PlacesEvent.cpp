/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PlacesEvent.h"

#include "mozilla/HoldDropJSObjects.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PlacesEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PlacesEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PlacesEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(PlacesEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PlacesEvent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PlacesEvent, Release)

already_AddRefed<PlacesEvent>
PlacesEvent::Constructor(const GlobalObject& aGlobal,
                         PlacesEventType aType,
                         ErrorResult& rv)
{
  RefPtr<PlacesEvent> event = new PlacesEvent(aType);
  return event.forget();
}

nsISupports*
PlacesEvent::GetParentObject() const
{
  return nullptr;
}

JSObject*
PlacesEvent::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PlacesEvent_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
