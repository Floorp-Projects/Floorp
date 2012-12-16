/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabParent.h"

#include "Blob.h"
#include "IDBFactory.h"
#include "IndexedDBParent.h"
#include "mozIApplication.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/docshell/OfflineCacheUpdateParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "nsCOMPtr.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsEventDispatcher.h"
#include "nsEventStateManager.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsIContent.h"
#include "nsIDOMApplicationRegistry.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMWindow.h"
#include "nsIDialogCreator.h"
#include "nsIPromptFactory.h"
#include "nsIURI.h"
#include "nsIMozBrowserFrame.h"
#include "nsIScriptSecurityManager.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIWindowWatcher.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsSerializationHelper.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "StructuredCloneUtils.h"
#include "TabChild.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::services;
using namespace mozilla::widget;
using namespace mozilla::dom::indexedDB;

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

namespace mozilla {
namespace dom {

TabParent* sEventCapturer;

TabParent *TabParent::mIMETabParent = nullptr;

NS_IMPL_ISUPPORTS3(TabParent, nsITabParent, nsIAuthPromptProvider, nsISecureBrowserUI)

TabParent::TabParent(const TabContext& aContext)
  : TabContext(aContext)
  , mFrameElement(NULL)
  , mIMESelectionAnchor(0)
  , mIMESelectionFocus(0)
  , mIMEComposing(false)
  , mIMECompositionEnding(false)
  , mIMECompositionStart(0)
  , mIMESeqno(0)
  , mEventCaptureDepth(0)
  , mDimensions(0, 0)
  , mDPI(0)
  , mShown(false)
  , mIsDestroyed(false)
{
}

TabParent::~TabParent()
{
}

void
TabParent::SetOwnerElement(nsIDOMElement* aElement)
{
  mFrameElement = aElement;
  TryCacheDPI();
}

void
TabParent::Destroy()
{
  if (mIsDestroyed) {
    return;
  }

  // If this fails, it's most likely due to a content-process crash,
  // and auto-cleanup will kick in.  Otherwise, the child side will
  // destroy itself and send back __delete__().
  unused << SendDestroy();

  const InfallibleTArray<PIndexedDBParent*>& idbParents =
    ManagedPIndexedDBParent();
  for (uint32_t i = 0; i < idbParents.Length(); ++i) {
    static_cast<IndexedDBParent*>(idbParents[i])->Disconnect();
  }

  if (RenderFrameParent* frame = GetRenderFrame()) {
    frame->Destroy();
  }
  mIsDestroyed = true;
}

bool
TabParent::Recv__delete__()
{
  ContentParent* cp = static_cast<ContentParent*>(Manager());
  cp->NotifyTabDestroyed(this);
  return true;
}

void
TabParent::ActorDestroy(ActorDestroyReason why)
{
  if (sEventCapturer == this) {
    sEventCapturer = nullptr;
  }
  if (mIMETabParent == this) {
    mIMETabParent = nullptr;
  }
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (frameLoader) {
    ReceiveMessage(CHILD_PROCESS_SHUTDOWN_MESSAGE, false, nullptr, nullptr);
    frameLoader->DestroyChild();

    if (why == AbnormalShutdown) {
      nsCOMPtr<nsIObserverService> os = services::GetObserverService();
      if (os) {
        os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, frameLoader),
                            "oop-frameloader-crashed", nullptr);
      }
    }
  }
}

bool
TabParent::RecvMoveFocus(const bool& aForward)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    nsCOMPtr<nsIDOMElement> dummy;
    uint32_t type = aForward ? uint32_t(nsIFocusManager::MOVEFOCUS_FORWARD)
                             : uint32_t(nsIFocusManager::MOVEFOCUS_BACKWARD);
    fm->MoveFocus(nullptr, mFrameElement, type, nsIFocusManager::FLAG_BYKEY, 
                  getter_AddRefs(dummy));
  }
  return true;
}

bool
TabParent::RecvEvent(const RemoteDOMEvent& aEvent)
{
  nsCOMPtr<nsIDOMEvent> event = do_QueryInterface(aEvent.mEvent);
  NS_ENSURE_TRUE(event, true);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mFrameElement);
  NS_ENSURE_TRUE(target, true);

  bool dummy;
  target->DispatchEvent(event, &dummy);
  return true;
}

bool
TabParent::AnswerCreateWindow(PBrowserParent** retval)
{
    if (!mBrowserDOMWindow) {
        return false;
    }

    // Only non-app, non-browser processes may call CreateWindow.
    if (IsBrowserOrApp()) {
        return false;
    }

    // Get a new rendering area from the browserDOMWin.  We don't want
    // to be starting any loads here, so get it with a null URI.
    nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner;
    mBrowserDOMWindow->OpenURIInFrame(nullptr, nullptr,
                                      nsIBrowserDOMWindow::OPEN_NEWTAB,
                                      nsIBrowserDOMWindow::OPEN_NEW,
                                      getter_AddRefs(frameLoaderOwner));
    if (!frameLoaderOwner) {
        return false;
    }

    nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    if (!frameLoader) {
        return false;
    }

    *retval = frameLoader->GetRemoteBrowser();
    return true;
}

void
TabParent::LoadURL(nsIURI* aURI)
{
    if (mIsDestroyed) {
      return;
    }
    if (!mShown) {
      nsAutoCString spec;
      if (aURI) {
        aURI->GetSpec(spec);
      }
      NS_WARNING(nsPrintfCString("TabParent::LoadURL(%s) called before "
                                 "Show(). Ignoring LoadURL.\n", spec.get()).get());
      return;
    }

    nsCString spec;
    aURI->GetSpec(spec);

    unused << SendLoadURL(spec);
}

void
TabParent::Show(const nsIntSize& size)
{
    // sigh
    mShown = true;
    mDimensions = size;
    if (!mIsDestroyed) {
      unused << SendShow(size);
    }
}

void
TabParent::UpdateDimensions(const nsRect& rect, const nsIntSize& size)
{
  if (mIsDestroyed) {
    return;
  }
  unused << SendUpdateDimensions(rect, size);
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->NotifyDimensionsChanged(size.width, size.height);
  }
  mDimensions = size;
}

void
TabParent::UpdateFrame(const FrameMetrics& aFrameMetrics)
{
  if (!mIsDestroyed) {
    unused << SendUpdateFrame(aFrameMetrics);
  }
}

void TabParent::HandleDoubleTap(const nsIntPoint& aPoint)
{
  if (!mIsDestroyed) {
    unused << SendHandleDoubleTap(aPoint);
  }
}

void TabParent::HandleSingleTap(const nsIntPoint& aPoint)
{
  if (!mIsDestroyed) {
    unused << SendHandleSingleTap(aPoint);
  }
}

void TabParent::HandleLongTap(const nsIntPoint& aPoint)
{
  if (!mIsDestroyed) {
    unused << SendHandleLongTap(aPoint);
  }
}

void
TabParent::Activate()
{
  if (!mIsDestroyed) {
    unused << SendActivate();
  }
}

void
TabParent::Deactivate()
{
  if (!mIsDestroyed) {
    unused << SendDeactivate();
  }
}

NS_IMETHODIMP
TabParent::Init(nsIDOMWindow *window)
{
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetState(uint32_t *aState)
{
  NS_ENSURE_ARG(aState);
  NS_WARNING("SecurityState not valid here");
  *aState = 0;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetTooltipText(nsAString & aTooltipText)
{
  aTooltipText.Truncate();
  return NS_OK;
}

PDocumentRendererParent*
TabParent::AllocPDocumentRenderer(const nsRect& documentRect,
                                  const gfxMatrix& transform,
                                  const nsString& bgcolor,
                                  const uint32_t& renderFlags,
                                  const bool& flushLayout,
                                  const nsIntSize& renderSize)
{
    return new DocumentRendererParent();
}

bool
TabParent::DeallocPDocumentRenderer(PDocumentRendererParent* actor)
{
    delete actor;
    return true;
}

PContentPermissionRequestParent*
TabParent::AllocPContentPermissionRequest(const nsCString& type, const nsCString& access, const IPC::Principal& principal)
{
  return new ContentPermissionRequestParent(type, access, mFrameElement, principal);
}

bool
TabParent::DeallocPContentPermissionRequest(PContentPermissionRequestParent* actor)
{
  delete actor;
  return true;
}

void
TabParent::SendMouseEvent(const nsAString& aType, float aX, float aY,
                          int32_t aButton, int32_t aClickCount,
                          int32_t aModifiers, bool aIgnoreRootScrollFrame)
{
  if (!mIsDestroyed) {
    unused << PBrowserParent::SendMouseEvent(nsString(aType), aX, aY,
                                             aButton, aClickCount,
                                             aModifiers, aIgnoreRootScrollFrame);
  }
}

void
TabParent::SendKeyEvent(const nsAString& aType,
                        int32_t aKeyCode,
                        int32_t aCharCode,
                        int32_t aModifiers,
                        bool aPreventDefault)
{
  if (!mIsDestroyed) {
    unused << PBrowserParent::SendKeyEvent(nsString(aType), aKeyCode, aCharCode,
                                           aModifiers, aPreventDefault);
  }
}

bool TabParent::SendRealMouseEvent(nsMouseEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  nsMouseEvent e(event);
  MaybeForwardEventToRenderFrame(event, &e);
  return PBrowserParent::SendRealMouseEvent(e);
}

bool TabParent::SendMouseWheelEvent(WheelEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  WheelEvent e(event);
  MaybeForwardEventToRenderFrame(event, &e);
  return PBrowserParent::SendMouseWheelEvent(event);
}

bool TabParent::SendRealKeyEvent(nsKeyEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  nsKeyEvent e(event);
  MaybeForwardEventToRenderFrame(event, &e);
  return PBrowserParent::SendRealKeyEvent(e);
}

bool TabParent::SendRealTouchEvent(nsTouchEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  if (event.message == NS_TOUCH_START) {
    MOZ_ASSERT((!sEventCapturer && mEventCaptureDepth == 0) ||
               (sEventCapturer == this && mEventCaptureDepth > 0));
    // We want to capture all remaining touch events in this series
    // for fast-path dispatch.
    sEventCapturer = this;
    ++mEventCaptureDepth;
  }

  nsTouchEvent e(event);
  // PresShell::HandleEventInternal adds touches on touch end/cancel,
  // when we're not capturing raw events from the widget backend.
  // This hack filters those out. Bug 785554
  if (sEventCapturer != this &&
      (event.message == NS_TOUCH_END || event.message == NS_TOUCH_CANCEL)) {
    for (int i = e.touches.Length() - 1; i >= 0; i--) {
      if (!e.touches[i]->mChanged)
        e.touches.RemoveElementAt(i);
    }
  }

  MaybeForwardEventToRenderFrame(event, &e);
  return (e.message == NS_TOUCH_MOVE) ?
    PBrowserParent::SendRealTouchMoveEvent(e) :
    PBrowserParent::SendRealTouchEvent(e);
}

/*static*/ TabParent*
TabParent::GetEventCapturer()
{
  return sEventCapturer;
}

bool
TabParent::TryCapture(const nsGUIEvent& aEvent)
{
  MOZ_ASSERT(sEventCapturer == this && mEventCaptureDepth > 0);

  if (aEvent.eventStructType != NS_TOUCH_EVENT) {
    // Only capture of touch events is implemented, for now.
    return false;
  }

  nsTouchEvent event(static_cast<const nsTouchEvent&>(aEvent));

  bool isTouchPointUp = (event.message == NS_TOUCH_END ||
                         event.message == NS_TOUCH_CANCEL);
  if (event.message == NS_TOUCH_START || isTouchPointUp) {
    // Let the DOM see touch start/end events so that its touch-point
    // state stays consistent.
    if (isTouchPointUp && 0 == --mEventCaptureDepth) {
      // All event series are un-captured, don't try to catch any
      // more.
      sEventCapturer = nullptr;
    }
    return false;
  }

  // Adjust the widget coordinates to be relative to our frame.
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();

  if (!frameLoader) {
    // No frame anymore?
    sEventCapturer = nullptr;
    return false;
  }

  nsEventStateManager::MapEventCoordinatesForChildProcess(frameLoader, &event);

  SendRealTouchEvent(event);
  return true;
}

bool
TabParent::RecvSyncMessage(const nsString& aMessage,
                           const ClonedMessageData& aData,
                           InfallibleTArray<nsString>* aJSONRetVal)
{
  const SerializedStructuredCloneBuffer& buffer = aData.data();
  const InfallibleTArray<PBlobParent*>& blobParents = aData.blobsParent();
  StructuredCloneData cloneData;
  cloneData.mData = buffer.data;
  cloneData.mDataLength = buffer.dataLength;
  if (!blobParents.IsEmpty()) {
    uint32_t length = blobParents.Length();
    cloneData.mClosure.mBlobs.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      BlobParent* blobParent = static_cast<BlobParent*>(blobParents[i]);
      MOZ_ASSERT(blobParent);
      nsCOMPtr<nsIDOMBlob> blob = blobParent->GetBlob();
      MOZ_ASSERT(blob);
      cloneData.mClosure.mBlobs.AppendElement(blob);
    }
  }
  return ReceiveMessage(aMessage, true, &cloneData, aJSONRetVal);
}

bool
TabParent::RecvAsyncMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData)
{
    const SerializedStructuredCloneBuffer& buffer = aData.data();
  const InfallibleTArray<PBlobParent*>& blobParents = aData.blobsParent();

    StructuredCloneData cloneData;
    cloneData.mData = buffer.data;
    cloneData.mDataLength = buffer.dataLength;

  if (!blobParents.IsEmpty()) {
    uint32_t length = blobParents.Length();
      cloneData.mClosure.mBlobs.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      BlobParent* blobParent = static_cast<BlobParent*>(blobParents[i]);
      MOZ_ASSERT(blobParent);
      nsCOMPtr<nsIDOMBlob> blob = blobParent->GetBlob();
        MOZ_ASSERT(blob);
        cloneData.mClosure.mBlobs.AppendElement(blob);
      }
    }
  return ReceiveMessage(aMessage, false, &cloneData, nullptr);
}

bool
TabParent::RecvSetCursor(const uint32_t& aCursor)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SetCursor((nsCursor) aCursor);
  }
  return true;
}

bool
TabParent::RecvSetBackgroundColor(const nscolor& aColor)
{
  if (RenderFrameParent* frame = GetRenderFrame()) {
    frame->SetBackgroundColor(aColor);
  }
  return true;
}

bool
TabParent::RecvNotifyIMEFocus(const bool& aFocus,
                              nsIMEUpdatePreference* aPreference,
                              uint32_t* aSeqno)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    aPreference->mWantUpdates = false;
    aPreference->mWantHints = false;
    return true;
  }

  *aSeqno = mIMESeqno;
  mIMETabParent = aFocus ? this : nullptr;
  mIMESelectionAnchor = 0;
  mIMESelectionFocus = 0;
  widget->OnIMEFocusChange(aFocus);

  if (aFocus) {
    *aPreference = widget->GetIMEUpdatePreference();
  } else {
    mIMECacheText.Truncate(0);
  }
  return true;
}

bool
TabParent::RecvNotifyIMETextChange(const uint32_t& aStart,
                                   const uint32_t& aEnd,
                                   const uint32_t& aNewEnd)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

  widget->OnIMETextChange(aStart, aEnd, aNewEnd);
  return true;
}

bool
TabParent::RecvNotifyIMESelection(const uint32_t& aSeqno,
                                  const uint32_t& aAnchor,
                                  const uint32_t& aFocus)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

  if (aSeqno == mIMESeqno) {
    mIMESelectionAnchor = aAnchor;
    mIMESelectionFocus = aFocus;
    widget->OnIMESelectionChange();
  }
  return true;
}

bool
TabParent::RecvNotifyIMETextHint(const nsString& aText)
{
  // Replace our cache with new text
  mIMECacheText = aText;
  return true;
}

/**
 * Try to answer query event using cached text.
 *
 * For NS_QUERY_SELECTED_TEXT, fail if the cache doesn't contain the whole
 *  selected range. (This shouldn't happen because PuppetWidget should have
 *  already sent the whole selection.)
 *
 * For NS_QUERY_TEXT_CONTENT, fail only if the cache doesn't overlap with
 *  the queried range. Note the difference from above. We use
 *  this behavior because a normal NS_QUERY_TEXT_CONTENT event is allowed to
 *  have out-of-bounds offsets, so that widget can request content without
 *  knowing the exact length of text. It's up to widget to handle cases when
 *  the returned offset/length are different from the queried offset/length.
 */
bool
TabParent::HandleQueryContentEvent(nsQueryContentEvent& aEvent)
{
  aEvent.mSucceeded = false;
  aEvent.mWasAsync = false;
  aEvent.mReply.mFocusedWidget = nsCOMPtr<nsIWidget>(GetWidget()).get();

  switch (aEvent.message)
  {
  case NS_QUERY_SELECTED_TEXT:
    {
      aEvent.mReply.mOffset = NS_MIN(mIMESelectionAnchor, mIMESelectionFocus);
      if (mIMESelectionAnchor == mIMESelectionFocus) {
        aEvent.mReply.mString.Truncate(0);
      } else {
        if (mIMESelectionAnchor > mIMECacheText.Length() ||
            mIMESelectionFocus > mIMECacheText.Length()) {
          break;
        }
        uint32_t selLen = mIMESelectionAnchor > mIMESelectionFocus ?
                          mIMESelectionAnchor - mIMESelectionFocus :
                          mIMESelectionFocus - mIMESelectionAnchor;
        aEvent.mReply.mString = Substring(mIMECacheText,
                                          aEvent.mReply.mOffset,
                                          selLen);
      }
      aEvent.mReply.mReversed = mIMESelectionFocus < mIMESelectionAnchor;
      aEvent.mReply.mHasSelection = true;
      aEvent.mSucceeded = true;
    }
    break;
  case NS_QUERY_TEXT_CONTENT:
    {
      uint32_t inputOffset = aEvent.mInput.mOffset,
               inputEnd = inputOffset + aEvent.mInput.mLength;

      if (inputEnd > mIMECacheText.Length()) {
        inputEnd = mIMECacheText.Length();
      }
      if (inputEnd < inputOffset) {
        break;
      }
      aEvent.mReply.mOffset = inputOffset;
      aEvent.mReply.mString = Substring(mIMECacheText,
                                        inputOffset,
                                        inputEnd - inputOffset);
      aEvent.mSucceeded = true;
    }
    break;
  }
  return true;
}

bool
TabParent::SendCompositionEvent(nsCompositionEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  mIMEComposing = event.message != NS_COMPOSITION_END;
  mIMECompositionStart = NS_MIN(mIMESelectionAnchor, mIMESelectionFocus);
  if (mIMECompositionEnding)
    return true;
  event.seqno = ++mIMESeqno;
  return PBrowserParent::SendCompositionEvent(event);
}

/**
 * During ResetInputState or CancelComposition, widget usually sends a
 * NS_TEXT_TEXT event to finalize or clear the composition, respectively
 *
 * Because the event will not reach content in time, we intercept it
 * here and pass the text as the EndIMEComposition return value
 */
bool
TabParent::SendTextEvent(nsTextEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  if (mIMECompositionEnding) {
    mIMECompositionText = event.theText;
    return true;
  }

  // We must be able to simulate the selection because
  // we might not receive selection updates in time
  if (!mIMEComposing) {
    mIMECompositionStart = NS_MIN(mIMESelectionAnchor, mIMESelectionFocus);
  }
  mIMESelectionAnchor = mIMESelectionFocus =
      mIMECompositionStart + event.theText.Length();

  event.seqno = ++mIMESeqno;
  return PBrowserParent::SendTextEvent(event);
}

bool
TabParent::SendSelectionEvent(nsSelectionEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  mIMESelectionAnchor = event.mOffset + (event.mReversed ? event.mLength : 0);
  mIMESelectionFocus = event.mOffset + (!event.mReversed ? event.mLength : 0);
  event.seqno = ++mIMESeqno;
  return PBrowserParent::SendSelectionEvent(event);
}

/*static*/ TabParent*
TabParent::GetFrom(nsFrameLoader* aFrameLoader)
{
  if (!aFrameLoader) {
    return nullptr;
  }
  PBrowserParent* remoteBrowser = aFrameLoader->GetRemoteBrowser();
  return static_cast<TabParent*>(remoteBrowser);
}

/*static*/ TabParent*
TabParent::GetFrom(nsIContent* aContent)
{
  nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(aContent);
  if (!loaderOwner) {
    return nullptr;
  }
  nsRefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  return GetFrom(frameLoader);
}

RenderFrameParent*
TabParent::GetRenderFrame()
{
  if (ManagedPRenderFrameParent().IsEmpty()) {
    return nullptr;
  }
  return static_cast<RenderFrameParent*>(ManagedPRenderFrameParent()[0]);
}

bool
TabParent::RecvEndIMEComposition(const bool& aCancel,
                                 nsString* aComposition)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

  mIMECompositionEnding = true;

  if (aCancel) {
    widget->CancelIMEComposition();
  } else {
    widget->ResetInputState();
  }

  mIMECompositionEnding = false;
  *aComposition = mIMECompositionText;
  mIMECompositionText.Truncate(0);  
  return true;
}

bool
TabParent::RecvGetInputContext(int32_t* aIMEEnabled,
                               int32_t* aIMEOpen,
                               intptr_t* aNativeIMEContext)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aIMEEnabled = IMEState::DISABLED;
    *aIMEOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    *aNativeIMEContext = 0;
    return true;
  }

  InputContext context = widget->GetInputContext();
  *aIMEEnabled = static_cast<int32_t>(context.mIMEState.mEnabled);
  *aIMEOpen = static_cast<int32_t>(context.mIMEState.mOpen);
  *aNativeIMEContext = reinterpret_cast<intptr_t>(context.mNativeIMEContext);
  return true;
}

bool
TabParent::RecvSetInputContext(const int32_t& aIMEEnabled,
                               const int32_t& aIMEOpen,
                               const nsString& aType,
                               const nsString& aInputmode,
                               const nsString& aActionHint,
                               const int32_t& aCause,
                               const int32_t& aFocusChange)
{
  // mIMETabParent (which is actually static) tracks which if any TabParent has IMEFocus
  // When the input mode is set to anything but IMEState::DISABLED,
  // mIMETabParent should be set to this
  mIMETabParent =
    aIMEEnabled != static_cast<int32_t>(IMEState::DISABLED) ? this : nullptr;
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget || !AllowContentIME())
    return true;

  InputContext context;
  context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(aIMEEnabled);
  context.mIMEState.mOpen = static_cast<IMEState::Open>(aIMEOpen);
  context.mHTMLInputType.Assign(aType);
  context.mHTMLInputInputmode.Assign(aInputmode);
  context.mActionHint.Assign(aActionHint);
  InputContextAction action(
    static_cast<InputContextAction::Cause>(aCause),
    static_cast<InputContextAction::FocusChange>(aFocusChange));
  widget->SetInputContext(context, action);

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (!observerService)
    return true;

  nsAutoString state;
  state.AppendInt(aIMEEnabled);
  observerService->NotifyObservers(nullptr, "ime-enabled-state-changed", state.get());

  return true;
}

bool
TabParent::RecvGetDPI(float* aValue)
{
  TryCacheDPI();

  NS_ABORT_IF_FALSE(mDPI > 0,
                    "Must not ask for DPI before OwnerElement is received!");
  *aValue = mDPI;
  return true;
}

bool
TabParent::RecvGetWidgetNativeData(WindowsHandle* aValue)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (content) {
    nsIPresShell* shell = content->OwnerDoc()->GetShell();
    if (shell) {
      nsIViewManager* vm = shell->GetViewManager();
      nsCOMPtr<nsIWidget> widget;
      vm->GetRootWidget(getter_AddRefs(widget));
      if (widget) {
        *aValue = reinterpret_cast<WindowsHandle>(
          widget->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW));
        return true;
      }
    }
  }
  return false;
}

bool
TabParent::ReceiveMessage(const nsString& aMessage,
                          bool aSync,
                          const StructuredCloneData* aCloneData,
                          InfallibleTArray<nsString>* aJSONRetVal)
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (frameLoader && frameLoader->GetFrameMessageManager()) {
    nsRefPtr<nsFrameMessageManager> manager =
      frameLoader->GetFrameMessageManager();
    JSContext* ctx = manager->GetJSContext();
    JSAutoRequest ar(ctx);
    uint32_t len = 0; //TODO: obtain a real value in bug 572685
    // Because we want JS messages to have always the same properties,
    // create array even if len == 0.
    JSObject* objectsArray = JS_NewArrayObject(ctx, len, NULL);
    if (!objectsArray) {
      return false;
    }

    manager->ReceiveMessage(mFrameElement,
                            aMessage,
                            aSync,
                            aCloneData,
                            objectsArray,
                            aJSONRetVal);
  }
  return true;
}

PIndexedDBParent*
TabParent::AllocPIndexedDB(const nsCString& aASCIIOrigin, bool* /* aAllowed */)
{
  return new IndexedDBParent(this);
}

bool
TabParent::DeallocPIndexedDB(PIndexedDBParent* aActor)
{
  delete aActor;
  return true;
}

bool
TabParent::RecvPIndexedDBConstructor(PIndexedDBParent* aActor,
                                     const nsCString& aASCIIOrigin,
                                     bool* aAllowed)
{
  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::GetOrCreate();
  NS_ENSURE_TRUE(mgr, false);

  if (!IndexedDatabaseManager::IsMainProcess()) {
    NS_RUNTIMEABORT("Not supported yet!");
  }

  nsresult rv;

  // XXXbent Need to make sure we have a whitelist for chrome databases!

  // Verify that the child is requesting to access a database it's allowed to
  // see.  (aASCIIOrigin here specifies a TabContext + a website origin, and
  // we're checking that the TabContext may access it.)
  //
  // We have to check IsBrowserOrApp() because TabContextMayAccessOrigin will
  // fail if we're not a browser-or-app, since aASCIIOrigin will be a plain URI,
  // but TabContextMayAccessOrigin will construct an extended origin using
  // app-id 0.  Note that as written below, we allow a non browser-or-app child
  // to read any database.  That's a security hole, but we don't ship a
  // configuration which creates non browser-or-app children, so it's not a big
  // deal.
  if (!aASCIIOrigin.EqualsLiteral("chrome") && IsBrowserOrApp() &&
      !IndexedDatabaseManager::TabContextMayAccessOrigin(*this, aASCIIOrigin)) {

    NS_WARNING("App attempted to open databases that it does not have "
               "permission to access!");
    return false;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(GetOwnerElement());
  NS_ENSURE_TRUE(node, false);

  nsIDocument* doc = node->GetOwnerDocument();
  NS_ENSURE_TRUE(doc, false);

  nsCOMPtr<nsPIDOMWindow> window = doc->GetInnerWindow();
  NS_ENSURE_TRUE(window, false);

  ContentParent* contentParent = static_cast<ContentParent*>(Manager());
  NS_ASSERTION(contentParent, "Null manager?!");

  nsRefPtr<IDBFactory> factory;
  rv = IDBFactory::Create(window, aASCIIOrigin, contentParent,
                          getter_AddRefs(factory));
  NS_ENSURE_SUCCESS(rv, false);

  if (!factory) {
    *aAllowed = false;
    return true;
  }

  IndexedDBParent* actor = static_cast<IndexedDBParent*>(aActor);
  actor->mFactory = factory;
  actor->mASCIIOrigin = aASCIIOrigin;

  *aAllowed = true;
  return true;
}

// nsIAuthPromptProvider

// This method is largely copied from nsDocShell::GetAuthPrompt
NS_IMETHODIMP
TabParent::GetAuthPrompt(uint32_t aPromptReason, const nsIID& iid,
                          void** aResult)
{
  // we're either allowing auth, or it's a proxy request
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> window;
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (frame)
    window = do_QueryInterface(frame->OwnerDoc()->GetWindow());

  // Get an auth prompter for our window so that the parenting
  // of the dialogs works as it should when using tabs.
  return wwatch->GetPrompt(window, iid,
                           reinterpret_cast<void**>(aResult));
}

PContentDialogParent*
TabParent::AllocPContentDialog(const uint32_t& aType,
                               const nsCString& aName,
                               const nsCString& aFeatures,
                               const InfallibleTArray<int>& aIntParams,
                               const InfallibleTArray<nsString>& aStringParams)
{
  ContentDialogParent* parent = new ContentDialogParent();
  nsCOMPtr<nsIDialogParamBlock> params =
    do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  TabChild::ArraysToParams(aIntParams, aStringParams, params);
  mDelayedDialogs.AppendElement(new DelayedDialogData(parent, aType, aName,
                                                      aFeatures, params));
  nsRefPtr<nsIRunnable> ev =
    NS_NewRunnableMethod(this, &TabParent::HandleDelayedDialogs);
  NS_DispatchToCurrentThread(ev);
  return parent;
}

void
TabParent::HandleDelayedDialogs()
{
  nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  nsCOMPtr<nsIDOMWindow> window;
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (frame) {
    window = do_QueryInterface(frame->OwnerDoc()->GetWindow());
  }
  nsCOMPtr<nsIDialogCreator> dialogCreator = do_QueryInterface(mBrowserDOMWindow);
  while (!ShouldDelayDialogs() && mDelayedDialogs.Length()) {
    uint32_t index = mDelayedDialogs.Length() - 1;
    DelayedDialogData* data = mDelayedDialogs[index];
    mDelayedDialogs.RemoveElementAt(index);
    nsCOMPtr<nsIDialogParamBlock> params;
    params.swap(data->mParams);
    PContentDialogParent* dialog = data->mDialog;
    if (dialogCreator) {
      dialogCreator->OpenDialog(data->mType,
                                data->mName, data->mFeatures,
                                params, mFrameElement);
    } else if (ww) {
      nsAutoCString url;
      if (data->mType) {
        if (data->mType == nsIDialogCreator::SELECT_DIALOG) {
          url.Assign("chrome://global/content/selectDialog.xul");
        } else if (data->mType == nsIDialogCreator::GENERIC_DIALOG) {
          url.Assign("chrome://global/content/commonDialog.xul");
        }

        nsCOMPtr<nsISupports> arguments(do_QueryInterface(params));
        nsCOMPtr<nsIDOMWindow> dialog;
        ww->OpenWindow(window, url.get(), data->mName.get(),
                       data->mFeatures.get(), arguments, getter_AddRefs(dialog));
      } else {
        NS_WARNING("unknown dialog types aren't automatically supported in E10s yet!");
      }
    }

    delete data;
    if (dialog) {
      InfallibleTArray<int32_t> intParams;
      InfallibleTArray<nsString> stringParams;
      TabChild::ParamsToArrays(params, intParams, stringParams);
      unused << PContentDialogParent::Send__delete__(dialog,
                                                     intParams, stringParams);
    }
  }
  if (ShouldDelayDialogs() && mDelayedDialogs.Length()) {
    nsContentUtils::DispatchTrustedEvent(frame->OwnerDoc(), frame,
                                         NS_LITERAL_STRING("MozDelayedModalDialog"),
                                         true, true);
  }
}

PRenderFrameParent*
TabParent::AllocPRenderFrame(ScrollingBehavior* aScrolling,
                             LayersBackend* aBackend,
                             int32_t* aMaxTextureSize,
                             uint64_t* aLayersId)
{
  MOZ_ASSERT(ManagedPRenderFrameParent().IsEmpty());

  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    NS_ERROR("Can't allocate graphics resources, aborting subprocess");
    return nullptr;
  }

  *aScrolling = UseAsyncPanZoom() ? ASYNC_PAN_ZOOM : DEFAULT_SCROLLING;
  return new RenderFrameParent(frameLoader,
                               *aScrolling,
                               aBackend, aMaxTextureSize, aLayersId);
}

bool
TabParent::DeallocPRenderFrame(PRenderFrameParent* aFrame)
{
  delete aFrame;
  return true;
}

mozilla::docshell::POfflineCacheUpdateParent*
TabParent::AllocPOfflineCacheUpdate(const URIParams& aManifestURI,
                                    const URIParams& aDocumentURI,
                                    const bool& isInBrowserElement,
                                    const uint32_t& appId,
                                    const bool& stickDocument)
{
  nsRefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
    new mozilla::docshell::OfflineCacheUpdateParent();

  nsresult rv = update->Schedule(aManifestURI, aDocumentURI,
                                 isInBrowserElement, appId, stickDocument);
  if (NS_FAILED(rv))
    return nullptr;

  POfflineCacheUpdateParent* result = update.get();
  update.forget();
  return result;
}

bool
TabParent::DeallocPOfflineCacheUpdate(mozilla::docshell::POfflineCacheUpdateParent* actor)
{
  mozilla::docshell::OfflineCacheUpdateParent* update =
    static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(actor);

  update->Release();
  return true;
}

bool
TabParent::ShouldDelayDialogs()
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  NS_ENSURE_TRUE(frameLoader, true);
  bool delay = false;
  frameLoader->GetDelayRemoteDialogs(&delay);
  return delay;
}

bool
TabParent::AllowContentIME()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedContent();
  if (focusedContent && focusedContent->IsEditable())
    return false;

  return true;
}

already_AddRefed<nsFrameLoader>
TabParent::GetFrameLoader() const
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(mFrameElement);
  return frameLoaderOwner ? frameLoaderOwner->GetFrameLoader() : nullptr;
}

void
TabParent::TryCacheDPI()
{
  if (mDPI > 0) {
    return;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();

  if (!widget && mFrameElement) {
    // Even if we don't have a widget (e.g. because we're display:none), there's
    // probably a widget somewhere in the hierarchy our frame element lives in.
    nsCOMPtr<nsIDOMDocument> ownerDoc;
    mFrameElement->GetOwnerDocument(getter_AddRefs(ownerDoc));

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(ownerDoc);
    widget = nsContentUtils::WidgetForDocument(doc);
  }

  if (widget) {
    mDPI = widget->GetDPI();
  }
}

already_AddRefed<nsIWidget>
TabParent::GetWidget() const
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (!content)
    return nullptr;

  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame)
    return nullptr;

  nsCOMPtr<nsIWidget> widget = frame->GetNearestWidget();
  return widget.forget();
}

bool
TabParent::UseAsyncPanZoom()
{
  bool usingOffMainThreadCompositing = !!CompositorParent::CompositorLoop();
  bool asyncPanZoomEnabled =
    Preferences::GetBool("layers.async-pan-zoom.enabled", false);
  return (usingOffMainThreadCompositing && asyncPanZoomEnabled &&
          GetScrollingBehavior() == ASYNC_PAN_ZOOM);
}

void
TabParent::MaybeForwardEventToRenderFrame(const nsInputEvent& aEvent,
                                          nsInputEvent* aOutEvent)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->NotifyInputEvent(aEvent, aOutEvent);
  }
}

bool
TabParent::RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                                      const nsString& aURL,
                                      const nsString& aName,
                                      const nsString& aFeatures,
                                      bool* aOutWindowOpened)
{
  *aOutWindowOpened =
    BrowserElementParent::OpenWindowOOP(static_cast<TabParent*>(aOpener),
                                        this, aURL, aName, aFeatures);
  return true;
}

bool
TabParent::RecvPRenderFrameConstructor(PRenderFrameParent* actor,
                                       ScrollingBehavior* scrolling,
                                       LayersBackend* backend,
                                       int32_t* maxTextureSize,
                                       uint64_t* layersId)
{
  RenderFrameParent* rfp = GetRenderFrame();
  if (mDimensions != nsIntSize() && rfp) {
    rfp->NotifyDimensionsChanged(mDimensions.width, mDimensions.height);
  }

  return true;
}

bool
TabParent::RecvZoomToRect(const gfxRect& aRect)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->ZoomToRect(aRect);
  }
  return true;
}

bool
TabParent::RecvUpdateZoomConstraints(const bool& aAllowZoom,
                                     const float& aMinZoom,
                                     const float& aMaxZoom)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->UpdateZoomConstraints(aAllowZoom, aMinZoom, aMaxZoom);
  }
  return true;
}

bool
TabParent::RecvContentReceivedTouch(const bool& aPreventDefault)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->ContentReceivedTouch(aPreventDefault);
  }
  return true;
}

} // namespace tabs
} // namespace mozilla
