/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsDOMUIEvent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsEventStateManager.h"
#include "nsIFrame.h"
#include "mozilla/Util.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/TextEvents.h"
#include "prtime.h"

using namespace mozilla;

nsDOMUIEvent::nsDOMUIEvent(mozilla::dom::EventTarget* aOwner,
                           nsPresContext* aPresContext, nsGUIEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent ?
               static_cast<nsEvent *>(aEvent) :
               static_cast<nsEvent *>(new nsUIEvent(false, 0, 0)))
  , mClientPoint(0, 0), mLayerPoint(0, 0), mPagePoint(0, 0), mMovementPoint(0, 0)
  , mIsPointerLocked(nsEventStateManager::sIsPointerLocked)
  , mLastClientPoint(nsEventStateManager::sLastClientPoint)
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
  
  // Fill mDetail and mView according to the mEvent (widget-generated
  // event) we've got
  switch(mEvent->eventStructType)
  {
    case NS_UI_EVENT:
    {
      nsUIEvent *event = static_cast<nsUIEvent*>(mEvent);
      mDetail = event->detail;
      break;
    }

    case NS_SCROLLPORT_EVENT:
    {
      InternalScrollPortEvent* scrollEvent =
        static_cast<InternalScrollPortEvent*>(mEvent);
      mDetail = (int32_t)scrollEvent->orient;
      break;
    }

    default:
      mDetail = 0;
      break;
  }

  mView = nullptr;
  if (mPresContext)
  {
    nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
    if (container)
    {
       nsCOMPtr<nsIDOMWindow> window = do_GetInterface(container);
       if (window)
          mView = do_QueryInterface(window);
    }
  }
}

//static
already_AddRefed<nsDOMUIEvent>
nsDOMUIEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                          const nsAString& aType,
                          const mozilla::dom::UIEventInit& aParam,
                          mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMUIEvent> e = new nsDOMUIEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  aRv = e->InitUIEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                       aParam.mDetail);
  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(nsDOMUIEvent, nsDOMEvent,
                                     mView)

NS_IMPL_ADDREF_INHERITED(nsDOMUIEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMUIEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUIEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

static nsIntPoint
DevPixelsToCSSPixels(const LayoutDeviceIntPoint& aPoint,
                     nsPresContext* aContext)
{
  return nsIntPoint(aContext->DevPixelsToIntCSSPixels(aPoint.x),
                    aContext->DevPixelsToIntCSSPixels(aPoint.y));
}

nsIntPoint
nsDOMUIEvent::GetMovementPoint()
{
  if (mPrivateDataDuplicated) {
    return mMovementPoint;
  }

  if (!mEvent ||
      (mEvent->eventStructType != NS_MOUSE_EVENT &&
       mEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
       mEvent->eventStructType != NS_WHEEL_EVENT &&
       mEvent->eventStructType != NS_DRAG_EVENT &&
       mEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
       !(static_cast<nsGUIEvent*>(mEvent)->widget)) {
    return nsIntPoint(0, 0);
  }

  // Calculate the delta between the last screen point and the current one.
  nsIntPoint current = DevPixelsToCSSPixels(mEvent->refPoint, mPresContext);
  nsIntPoint last = DevPixelsToCSSPixels(mEvent->lastRefPoint, mPresContext);
  return current - last;
}

NS_IMETHODIMP
nsDOMUIEvent::GetView(nsIDOMWindow** aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetDetail(int32_t* aDetail)
{
  *aDetail = mDetail;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::InitUIEvent(const nsAString& typeArg,
                          bool canBubbleArg,
                          bool cancelableArg,
                          nsIDOMWindow* viewArg,
                          int32_t detailArg)
{
  if (viewArg) {
    nsCOMPtr<nsPIDOMWindow> view = do_QueryInterface(viewArg);
    NS_ENSURE_TRUE(view, NS_ERROR_INVALID_ARG);
  }
  nsresult rv = nsDOMEvent::InitEvent(typeArg, canBubbleArg, cancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mDetail = detailArg;
  mView = viewArg;

  return NS_OK;
}

// ---- nsDOMNSUIEvent implementation -------------------
NS_IMETHODIMP
nsDOMUIEvent::GetPageX(int32_t* aPageX)
{
  NS_ENSURE_ARG_POINTER(aPageX);
  *aPageX = PageX();
  return NS_OK;
}

int32_t
nsDOMUIEvent::PageX() const
{
  if (mPrivateDataDuplicated) {
    return mPagePoint.x;
  }

  return nsDOMEvent::GetPageCoords(mPresContext,
                                   mEvent,
                                   mEvent->refPoint,
                                   mClientPoint).x;
}

NS_IMETHODIMP
nsDOMUIEvent::GetPageY(int32_t* aPageY)
{
  NS_ENSURE_ARG_POINTER(aPageY);
  *aPageY = PageY();
  return NS_OK;
}

int32_t
nsDOMUIEvent::PageY() const
{
  if (mPrivateDataDuplicated) {
    return mPagePoint.y;
  }

  return nsDOMEvent::GetPageCoords(mPresContext,
                                   mEvent,
                                   mEvent->refPoint,
                                   mClientPoint).y;
}

NS_IMETHODIMP
nsDOMUIEvent::GetWhich(uint32_t* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);
  *aWhich = Which();
  return NS_OK;
}

already_AddRefed<nsINode>
nsDOMUIEvent::GetRangeParent()
{
  nsIFrame* targetFrame = nullptr;

  if (mPresContext) {
    targetFrame = mPresContext->EventStateManager()->GetEventTarget();
  }

  if (targetFrame) {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent,
                                                              targetFrame);
    nsCOMPtr<nsIContent> parent = targetFrame->GetContentOffsetsFromPoint(pt).content;
    if (parent) {
      if (parent->ChromeOnlyAccess() &&
          !nsContentUtils::CanAccessNativeAnon()) {
        return nullptr;
      }
      return parent.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMUIEvent::GetRangeParent(nsIDOMNode** aRangeParent)
{
  NS_ENSURE_ARG_POINTER(aRangeParent);
  *aRangeParent = nullptr;
  nsCOMPtr<nsINode> n = GetRangeParent();
  if (n) {
    CallQueryInterface(n, aRangeParent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetRangeOffset(int32_t* aRangeOffset)
{
  NS_ENSURE_ARG_POINTER(aRangeOffset);
  *aRangeOffset = RangeOffset();
  return NS_OK;
}

int32_t
nsDOMUIEvent::RangeOffset() const
{
  if (!mPresContext) {
    return 0;
  }

  nsIFrame* targetFrame = mPresContext->EventStateManager()->GetEventTarget();
  if (!targetFrame) {
    return 0;
  }

  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent,
                                                            targetFrame);
  return targetFrame->GetContentOffsetsFromPoint(pt).offset;
}

NS_IMETHODIMP
nsDOMUIEvent::GetCancelBubble(bool* aCancelBubble)
{
  NS_ENSURE_ARG_POINTER(aCancelBubble);
  *aCancelBubble = CancelBubble();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::SetCancelBubble(bool aCancelBubble)
{
  mEvent->mFlags.mPropagationStopped = aCancelBubble;
  return NS_OK;
}

nsIntPoint
nsDOMUIEvent::GetLayerPoint() const
{
  if (!mEvent ||
      (mEvent->eventStructType != NS_MOUSE_EVENT &&
       mEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
       mEvent->eventStructType != NS_WHEEL_EVENT &&
       mEvent->eventStructType != NS_TOUCH_EVENT &&
       mEvent->eventStructType != NS_DRAG_EVENT &&
       mEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
      !mPresContext ||
      mEventIsInternal) {
    return mLayerPoint;
  }
  // XXX I'm not really sure this is correct; it's my best shot, though
  nsIFrame* targetFrame = mPresContext->EventStateManager()->GetEventTarget();
  if (!targetFrame)
    return mLayerPoint;
  nsIFrame* layer = nsLayoutUtils::GetClosestLayer(targetFrame);
  nsPoint pt(nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent, layer));
  return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                    nsPresContext::AppUnitsToIntCSSPixels(pt.y));
}

NS_IMETHODIMP
nsDOMUIEvent::GetLayerX(int32_t* aLayerX)
{
  NS_ENSURE_ARG_POINTER(aLayerX);
  *aLayerX = GetLayerPoint().x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetLayerY(int32_t* aLayerY)
{
  NS_ENSURE_ARG_POINTER(aLayerY);
  *aLayerY = GetLayerPoint().y;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetIsChar(bool* aIsChar)
{
  *aIsChar = IsChar();
  return NS_OK;
}

bool
nsDOMUIEvent::IsChar() const
{
  switch (mEvent->eventStructType)
  {
    case NS_KEY_EVENT:
      return static_cast<nsKeyEvent*>(mEvent)->isChar;
    case NS_TEXT_EVENT:
      return static_cast<nsTextEvent*>(mEvent)->isChar;
    default:
      return false;
  }
  MOZ_CRASH("Switch handles all cases.");
}

NS_IMETHODIMP
nsDOMUIEvent::DuplicatePrivateData()
{
  mClientPoint = nsDOMEvent::GetClientCoords(mPresContext,
                                             mEvent,
                                             mEvent->refPoint,
                                             mClientPoint);
  mMovementPoint = GetMovementPoint();
  mLayerPoint = GetLayerPoint();
  mPagePoint = nsDOMEvent::GetPageCoords(mPresContext,
                                         mEvent,
                                         mEvent->refPoint,
                                         mClientPoint);
  // GetScreenPoint converts mEvent->refPoint to right coordinates.
  nsIntPoint screenPoint = nsDOMEvent::GetScreenCoords(mPresContext,
                                                       mEvent,
                                                       mEvent->refPoint);
  nsresult rv = nsDOMEvent::DuplicatePrivateData();
  if (NS_SUCCEEDED(rv)) {
    mEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(screenPoint);
  }
  return rv;
}

NS_IMETHODIMP_(void)
nsDOMUIEvent::Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("uievent"));
  }

  nsDOMEvent::Serialize(aMsg, false);

  int32_t detail = 0;
  GetDetail(&detail);
  IPC::WriteParam(aMsg, detail);
}

NS_IMETHODIMP_(bool)
nsDOMUIEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ENSURE_TRUE(nsDOMEvent::Deserialize(aMsg, aIter), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &mDetail), false);
  return true;
}

// XXX Following struct and array are used only in
//     nsDOMUIEvent::ComputeModifierState(), but if we define them in it,
//     we fail to build on Mac at calling mozilla::ArrayLength().
struct nsModifierPair
{
  mozilla::Modifier modifier;
  const char* name;
};
static const nsModifierPair kPairs[] = {
  { MODIFIER_ALT,        NS_DOM_KEYNAME_ALT },
  { MODIFIER_ALTGRAPH,   NS_DOM_KEYNAME_ALTGRAPH },
  { MODIFIER_CAPSLOCK,   NS_DOM_KEYNAME_CAPSLOCK },
  { MODIFIER_CONTROL,    NS_DOM_KEYNAME_CONTROL },
  { MODIFIER_FN,         NS_DOM_KEYNAME_FN },
  { MODIFIER_META,       NS_DOM_KEYNAME_META },
  { MODIFIER_NUMLOCK,    NS_DOM_KEYNAME_NUMLOCK },
  { MODIFIER_SCROLLLOCK, NS_DOM_KEYNAME_SCROLLLOCK },
  { MODIFIER_SHIFT,      NS_DOM_KEYNAME_SHIFT },
  { MODIFIER_SYMBOLLOCK, NS_DOM_KEYNAME_SYMBOLLOCK },
  { MODIFIER_OS,         NS_DOM_KEYNAME_OS }
};

/* static */
mozilla::Modifiers
nsDOMUIEvent::ComputeModifierState(const nsAString& aModifiersList)
{
  if (aModifiersList.IsEmpty()) {
    return 0;
  }

  // Be careful about the performance.  If aModifiersList is too long,
  // parsing it needs too long time.
  // XXX Should we abort if aModifiersList is too long?

  Modifiers modifiers = 0;

  nsAString::const_iterator listStart, listEnd;
  aModifiersList.BeginReading(listStart);
  aModifiersList.EndReading(listEnd);

  for (uint32_t i = 0; i < mozilla::ArrayLength(kPairs); i++) {
    nsAString::const_iterator start(listStart), end(listEnd);
    if (!FindInReadable(NS_ConvertASCIItoUTF16(kPairs[i].name), start, end)) {
      continue;
    }

    if ((start != listStart && !NS_IsAsciiWhitespace(*(--start))) ||
        (end != listEnd && !NS_IsAsciiWhitespace(*(end)))) {
      continue;
    }
    modifiers |= kPairs[i].modifier;
  }

  return modifiers;
}

bool
nsDOMUIEvent::GetModifierStateInternal(const nsAString& aKey)
{
  if (!mEvent->IsInputDerivedEvent()) {
    MOZ_CRASH("mEvent must be nsInputEvent or derived class");
  }
  nsInputEvent* inputEvent = static_cast<nsInputEvent*>(mEvent);
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_SHIFT)) {
    return inputEvent->IsShift();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_CONTROL)) {
    return inputEvent->IsControl();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_META)) {
    return inputEvent->IsMeta();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_ALT)) {
    return inputEvent->IsAlt();
  }

  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_ALTGRAPH)) {
    return inputEvent->IsAltGraph();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_OS)) {
    return inputEvent->IsOS();
  }

  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_CAPSLOCK)) {
    return inputEvent->IsCapsLocked();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_NUMLOCK)) {
    return inputEvent->IsNumLocked();
  }

  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_FN)) {
    return inputEvent->IsFn();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_SCROLLLOCK)) {
    return inputEvent->IsScrollLocked();
  }
  if (aKey.EqualsLiteral(NS_DOM_KEYNAME_SYMBOLLOCK)) {
    return inputEvent->IsSymbolLocked();
  }
  return false;
}


nsresult NS_NewDOMUIEvent(nsIDOMEvent** aInstancePtrResult,
                          mozilla::dom::EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          nsGUIEvent *aEvent) 
{
  nsDOMUIEvent* it = new nsDOMUIEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
