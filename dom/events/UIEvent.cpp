/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/TextEvents.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIFrame.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

UIEvent::UIEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 WidgetGUIEvent* aEvent)
  : Event(aOwner, aPresContext,
          aEvent ? aEvent : new InternalUIEvent(false, eVoidEvent, nullptr))
  , mClientPoint(0, 0)
  , mLayerPoint(0, 0)
  , mPagePoint(0, 0)
  , mMovementPoint(0, 0)
  , mIsPointerLocked(EventStateManager::sIsPointerLocked)
  , mLastClientPoint(EventStateManager::sLastClientPoint)
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
  }
  
  // Fill mDetail and mView according to the mEvent (widget-generated
  // event) we've got
  switch(mEvent->mClass) {
    case eUIEventClass:
    {
      mDetail = mEvent->AsUIEvent()->mDetail;
      break;
    }

    case eScrollPortEventClass:
    {
      InternalScrollPortEvent* scrollEvent = mEvent->AsScrollPortEvent();
      mDetail = static_cast<int32_t>(scrollEvent->mOrient);
      break;
    }

    default:
      mDetail = 0;
      break;
  }

  mView = nullptr;
  if (mPresContext)
  {
    nsIDocShell* docShell = mPresContext->GetDocShell();
    if (docShell)
    {
       mView = docShell->GetWindow();
    }
  }
}

// static
already_AddRefed<UIEvent>
UIEvent::Constructor(const GlobalObject& aGlobal,
                     const nsAString& aType,
                     const UIEventInit& aParam,
                     ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<UIEvent> e = new UIEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitUIEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                 aParam.mDetail);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(UIEvent, Event,
                                   mView)

NS_IMPL_ADDREF_INHERITED(UIEvent, Event)
NS_IMPL_RELEASE_INHERITED(UIEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(UIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUIEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

static nsIntPoint
DevPixelsToCSSPixels(const LayoutDeviceIntPoint& aPoint,
                     nsPresContext* aContext)
{
  return nsIntPoint(aContext->DevPixelsToIntCSSPixels(aPoint.x),
                    aContext->DevPixelsToIntCSSPixels(aPoint.y));
}

nsIntPoint
UIEvent::GetMovementPoint()
{
  if (mEvent->mFlags.mIsPositionless) {
    return nsIntPoint(0, 0);
  }

  if (mPrivateDataDuplicated || mEventIsInternal) {
    return mMovementPoint;
  }

  if (!mEvent ||
      (mEvent->mClass != eMouseEventClass &&
       mEvent->mClass != eMouseScrollEventClass &&
       mEvent->mClass != eWheelEventClass &&
       mEvent->mClass != eDragEventClass &&
       mEvent->mClass != ePointerEventClass &&
       mEvent->mClass != eSimpleGestureEventClass) ||
       !mEvent->AsGUIEvent()->mWidget) {
    return nsIntPoint(0, 0);
  }

  // Calculate the delta between the last screen point and the current one.
  nsIntPoint current = DevPixelsToCSSPixels(mEvent->mRefPoint, mPresContext);
  nsIntPoint last = DevPixelsToCSSPixels(mEvent->mLastRefPoint, mPresContext);
  return current - last;
}

NS_IMETHODIMP
UIEvent::GetView(mozIDOMWindowProxy** aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP
UIEvent::GetDetail(int32_t* aDetail)
{
  *aDetail = mDetail;
  return NS_OK;
}

void
UIEvent::InitUIEvent(const nsAString& typeArg,
                     bool canBubbleArg,
                     bool cancelableArg,
                     nsGlobalWindow* viewArg,
                     int32_t detailArg)
{
  auto* view = viewArg ? viewArg->AsInner() : nullptr;
  InitUIEvent(typeArg, canBubbleArg, cancelableArg, view, detailArg);
}

NS_IMETHODIMP
UIEvent::InitUIEvent(const nsAString& typeArg,
                     bool canBubbleArg,
                     bool cancelableArg,
                     mozIDOMWindow* viewArg,
                     int32_t detailArg)
{
  NS_ENSURE_TRUE(!mEvent->mFlags.mIsBeingDispatched, NS_OK);

  Event::InitEvent(typeArg, canBubbleArg, cancelableArg);

  mDetail = detailArg;
  mView = viewArg ? nsPIDOMWindowInner::From(viewArg)->GetOuterWindow() :
                    nullptr;

  return NS_OK;
}

NS_IMETHODIMP
UIEvent::GetPageX(int32_t* aPageX)
{
  NS_ENSURE_ARG_POINTER(aPageX);
  *aPageX = PageX();
  return NS_OK;
}

int32_t
UIEvent::PageX() const
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (mPrivateDataDuplicated) {
    return mPagePoint.x;
  }

  return Event::GetPageCoords(mPresContext, mEvent, mEvent->mRefPoint,
                              mClientPoint).x;
}

NS_IMETHODIMP
UIEvent::GetPageY(int32_t* aPageY)
{
  NS_ENSURE_ARG_POINTER(aPageY);
  *aPageY = PageY();
  return NS_OK;
}

int32_t
UIEvent::PageY() const
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (mPrivateDataDuplicated) {
    return mPagePoint.y;
  }

  return Event::GetPageCoords(mPresContext, mEvent, mEvent->mRefPoint,
                              mClientPoint).y;
}

NS_IMETHODIMP
UIEvent::GetWhich(uint32_t* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);
  *aWhich = Which();
  return NS_OK;
}

already_AddRefed<nsINode>
UIEvent::GetRangeParent()
{
  nsIFrame* targetFrame = nullptr;

  if (mPresContext) {
    nsCOMPtr<nsIPresShell> shell = mPresContext->GetPresShell();
    if (shell) {
      shell->FlushPendingNotifications(FlushType::Layout);
      targetFrame = mPresContext->EventStateManager()->GetEventTarget();
    }
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
UIEvent::GetRangeParent(nsIDOMNode** aRangeParent)
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
UIEvent::GetRangeOffset(int32_t* aRangeOffset)
{
  NS_ENSURE_ARG_POINTER(aRangeOffset);
  *aRangeOffset = RangeOffset();
  return NS_OK;
}

int32_t
UIEvent::RangeOffset() const
{
  if (!mPresContext) {
    return 0;
  }

  nsCOMPtr<nsIPresShell> shell = mPresContext->GetPresShell();
  if (!shell) {
    return 0;
  }

  shell->FlushPendingNotifications(FlushType::Layout);

  nsIFrame* targetFrame = mPresContext->EventStateManager()->GetEventTarget();
  if (!targetFrame) {
    return 0;
  }

  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent,
                                                            targetFrame);
  return targetFrame->GetContentOffsetsFromPoint(pt).offset;
}

nsIntPoint
UIEvent::GetLayerPoint() const
{
  if (mEvent->mFlags.mIsPositionless) {
    return nsIntPoint(0, 0);
  }

  if (!mEvent ||
      (mEvent->mClass != eMouseEventClass &&
       mEvent->mClass != eMouseScrollEventClass &&
       mEvent->mClass != eWheelEventClass &&
       mEvent->mClass != ePointerEventClass &&
       mEvent->mClass != eTouchEventClass &&
       mEvent->mClass != eDragEventClass &&
       mEvent->mClass != eSimpleGestureEventClass) ||
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
UIEvent::GetLayerX(int32_t* aLayerX)
{
  NS_ENSURE_ARG_POINTER(aLayerX);
  *aLayerX = GetLayerPoint().x;
  return NS_OK;
}

NS_IMETHODIMP
UIEvent::GetLayerY(int32_t* aLayerY)
{
  NS_ENSURE_ARG_POINTER(aLayerY);
  *aLayerY = GetLayerPoint().y;
  return NS_OK;
}

mozilla::dom::Event*
UIEvent::AsEvent(void)
{
  return this;
}

NS_IMETHODIMP
UIEvent::DuplicatePrivateData()
{
  mClientPoint =
    Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                           mClientPoint);
  mMovementPoint = GetMovementPoint();
  mLayerPoint = GetLayerPoint();
  mPagePoint =
    Event::GetPageCoords(mPresContext, mEvent, mEvent->mRefPoint, mClientPoint);
  // GetScreenPoint converts mEvent->mRefPoint to right coordinates.
  CSSIntPoint screenPoint =
    Event::GetScreenCoords(mPresContext, mEvent, mEvent->mRefPoint);
  nsresult rv = Event::DuplicatePrivateData();
  if (NS_SUCCEEDED(rv)) {
    CSSToLayoutDeviceScale scale = mPresContext ? mPresContext->CSSToDevPixelScale()
                                                : CSSToLayoutDeviceScale(1);
    mEvent->mRefPoint = RoundedToInt(screenPoint * scale);
  }
  return rv;
}

NS_IMETHODIMP_(void)
UIEvent::Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("uievent"));
  }

  Event::Serialize(aMsg, false);

  int32_t detail = 0;
  GetDetail(&detail);
  IPC::WriteParam(aMsg, detail);
}

NS_IMETHODIMP_(bool)
UIEvent::Deserialize(const IPC::Message* aMsg, PickleIterator* aIter)
{
  NS_ENSURE_TRUE(Event::Deserialize(aMsg, aIter), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &mDetail), false);
  return true;
}

// XXX Following struct and array are used only in
//     UIEvent::ComputeModifierState(), but if we define them in it,
//     we fail to build on Mac at calling mozilla::ArrayLength().
struct ModifierPair
{
  Modifier modifier;
  const char* name;
};
static const ModifierPair kPairs[] = {
  { MODIFIER_ALT,        NS_DOM_KEYNAME_ALT },
  { MODIFIER_ALTGRAPH,   NS_DOM_KEYNAME_ALTGRAPH },
  { MODIFIER_CAPSLOCK,   NS_DOM_KEYNAME_CAPSLOCK },
  { MODIFIER_CONTROL,    NS_DOM_KEYNAME_CONTROL },
  { MODIFIER_FN,         NS_DOM_KEYNAME_FN },
  { MODIFIER_FNLOCK,     NS_DOM_KEYNAME_FNLOCK },
  { MODIFIER_META,       NS_DOM_KEYNAME_META },
  { MODIFIER_NUMLOCK,    NS_DOM_KEYNAME_NUMLOCK },
  { MODIFIER_SCROLLLOCK, NS_DOM_KEYNAME_SCROLLLOCK },
  { MODIFIER_SHIFT,      NS_DOM_KEYNAME_SHIFT },
  { MODIFIER_SYMBOL,     NS_DOM_KEYNAME_SYMBOL },
  { MODIFIER_SYMBOLLOCK, NS_DOM_KEYNAME_SYMBOLLOCK },
  { MODIFIER_OS,         NS_DOM_KEYNAME_OS }
};

// static
Modifiers
UIEvent::ComputeModifierState(const nsAString& aModifiersList)
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

  for (uint32_t i = 0; i < ArrayLength(kPairs); i++) {
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
UIEvent::GetModifierStateInternal(const nsAString& aKey)
{
  WidgetInputEvent* inputEvent = mEvent->AsInputEvent();
  MOZ_ASSERT(inputEvent, "mEvent must be WidgetInputEvent or derived class");
  return ((inputEvent->mModifiers & WidgetInputEvent::GetModifier(aKey)) != 0);
}

void
UIEvent::InitModifiers(const EventModifierInit& aParam)
{
  if (NS_WARN_IF(!mEvent)) {
    return;
  }
  WidgetInputEvent* inputEvent = mEvent->AsInputEvent();
  MOZ_ASSERT(inputEvent,
             "This method shouldn't be called if it doesn't have modifiers");
  if (NS_WARN_IF(!inputEvent)) {
    return;
  }

  inputEvent->mModifiers = MODIFIER_NONE;

#define SET_MODIFIER(aName, aValue) \
  if (aParam.m##aName) { \
    inputEvent->mModifiers |= aValue; \
  } \

  SET_MODIFIER(CtrlKey,                 MODIFIER_CONTROL)
  SET_MODIFIER(ShiftKey,                MODIFIER_SHIFT)
  SET_MODIFIER(AltKey,                  MODIFIER_ALT)
  SET_MODIFIER(MetaKey,                 MODIFIER_META)
  SET_MODIFIER(ModifierAltGraph,        MODIFIER_ALTGRAPH)
  SET_MODIFIER(ModifierCapsLock,        MODIFIER_CAPSLOCK)
  SET_MODIFIER(ModifierFn,              MODIFIER_FN)
  SET_MODIFIER(ModifierFnLock,          MODIFIER_FNLOCK)
  SET_MODIFIER(ModifierNumLock,         MODIFIER_NUMLOCK)
  SET_MODIFIER(ModifierOS,              MODIFIER_OS)
  SET_MODIFIER(ModifierScrollLock,      MODIFIER_SCROLLLOCK)
  SET_MODIFIER(ModifierSymbol,          MODIFIER_SYMBOL)
  SET_MODIFIER(ModifierSymbolLock,      MODIFIER_SYMBOLLOCK)

#undef SET_MODIFIER
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<UIEvent>
NS_NewDOMUIEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 WidgetGUIEvent* aEvent) 
{
  RefPtr<UIEvent> it = new UIEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
