/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DragEvent.h"
#include "mozilla/MouseEvents.h"
#include "nsContentUtils.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

DragEvent::DragEvent(EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     WidgetDragEvent* aEvent)
  : MouseEvent(aOwner, aPresContext,
               aEvent ? aEvent :
                        new WidgetDragEvent(false, eVoidEvent, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    mEvent->AsMouseEvent()->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

NS_IMPL_ADDREF_INHERITED(DragEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(DragEvent, MouseEvent)

NS_INTERFACE_MAP_BEGIN(DragEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDragEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

void
DragEvent::InitDragEvent(const nsAString& aType,
                         bool aCanBubble,
                         bool aCancelable,
                         nsGlobalWindow* aView,
                         int32_t aDetail,
                         int32_t aScreenX,
                         int32_t aScreenY,
                         int32_t aClientX,
                         int32_t aClientY,
                         bool aCtrlKey,
                         bool aAltKey,
                         bool aShiftKey,
                         bool aMetaKey,
                         uint16_t aButton,
                         EventTarget* aRelatedTarget,
                         DataTransfer* aDataTransfer)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                             aView, aDetail, aScreenX, aScreenY,
                             aClientX, aClientY, aCtrlKey, aAltKey,
                             aShiftKey, aMetaKey, aButton, aRelatedTarget);
  if (mEventIsInternal) {
    mEvent->AsDragEvent()->mDataTransfer = aDataTransfer;
  }
}

NS_IMETHODIMP
DragEvent::GetDataTransfer(nsIDOMDataTransfer** aDataTransfer)
{
  NS_IF_ADDREF(*aDataTransfer = GetDataTransfer());
  return NS_OK;
}

DataTransfer*
DragEvent::GetDataTransfer()
{
  // the dataTransfer field of the event caches the DataTransfer associated
  // with the drag. It is initialized when an attempt is made to retrieve it
  // rather that when the event is created to avoid duplicating the data when
  // no listener ever uses it.
  if (!mEvent || mEvent->mClass != eDragEventClass) {
    NS_WARNING("Tried to get dataTransfer from non-drag event!");
    return nullptr;
  }

  WidgetDragEvent* dragEvent = mEvent->AsDragEvent();
  // for synthetic events, just use the supplied data transfer object even if null
  if (!mEventIsInternal) {
    nsresult rv = nsContentUtils::SetDataTransferInEvent(dragEvent);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  return dragEvent->mDataTransfer;
}

// static
already_AddRefed<DragEvent>
DragEvent::Constructor(const GlobalObject& aGlobal,
                       const nsAString& aType,
                       const DragEventInit& aParam,
                       ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<DragEvent> e = new DragEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitDragEvent(aType, aParam.mBubbles, aParam.mCancelable,
                   aParam.mView, aParam.mDetail, aParam.mScreenX,
                   aParam.mScreenY, aParam.mClientX, aParam.mClientY,
                   aParam.mCtrlKey, aParam.mAltKey, aParam.mShiftKey,
                   aParam.mMetaKey, aParam.mButton, aParam.mRelatedTarget,
                   aParam.mDataTransfer);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<DragEvent>
NS_NewDOMDragEvent(EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   WidgetDragEvent* aEvent) 
{
  RefPtr<DragEvent> event =
    new DragEvent(aOwner, aPresContext, aEvent);
  return event.forget();
}
