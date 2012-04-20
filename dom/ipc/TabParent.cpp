/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TabParent.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/docshell/OfflineCacheUpdateParent.h"

#include "nsIURI.h"
#include "nsFocusManager.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsEventDispatcher.h"
#include "nsIDOMEventTarget.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "TabChild.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsFrameLoader.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsContentPermissionHelper.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDialogCreator.h"
#include "nsThreadUtils.h"
#include "nsSerializationHelper.h"
#include "nsIPromptFactory.h"
#include "nsIContent.h"
#include "nsIWidget.h"
#include "nsIViewManager.h"
#include "mozilla/unused.h"
#include "nsDebug.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layout;
using namespace mozilla::widget;

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

namespace mozilla {
namespace dom {

TabParent *TabParent::mIMETabParent = nsnull;

NS_IMPL_ISUPPORTS3(TabParent, nsITabParent, nsIAuthPromptProvider, nsISecureBrowserUI)

TabParent::TabParent()
  : mIMEComposing(false)
  , mIMECompositionEnding(false)
  , mIMESeqno(0)
  , mDPI(0)
  , mActive(false)
{
}

TabParent::~TabParent()
{
}

void
TabParent::SetOwnerElement(nsIDOMElement* aElement)
{
  mFrameElement = aElement;

  // Cache the DPI of the screen, since we may lose the element/widget later
  if (aElement) {
    nsCOMPtr<nsIWidget> widget = GetWidget();
    NS_ABORT_IF_FALSE(widget, "Non-null OwnerElement must provide a widget!");
    mDPI = widget->GetDPI();
  }
}

void
TabParent::Destroy()
{
  // If this fails, it's most likely due to a content-process crash,
  // and auto-cleanup will kick in.  Otherwise, the child side will
  // destroy itself and send back __delete__().
  unused << SendDestroy();

  for (size_t i = 0; i < ManagedPRenderFrameParent().Length(); ++i) {
    RenderFrameParent* rfp =
      static_cast<RenderFrameParent*>(ManagedPRenderFrameParent()[i]);
    rfp->Destroy();
  }
}

void
TabParent::ActorDestroy(ActorDestroyReason why)
{
  if (mIMETabParent == this)
    mIMETabParent = nsnull;
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (frameLoader) {
    frameLoader->DestroyChild();
  }
}

bool
TabParent::RecvMoveFocus(const bool& aForward)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    nsCOMPtr<nsIDOMElement> dummy;
    PRUint32 type = aForward ? PRUint32(nsIFocusManager::MOVEFOCUS_FORWARD)
                             : PRUint32(nsIFocusManager::MOVEFOCUS_BACKWARD);
    fm->MoveFocus(nsnull, mFrameElement, type, nsIFocusManager::FLAG_BYKEY, 
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

    // Get a new rendering area from the browserDOMWin.  We don't want
    // to be starting any loads here, so get it with a null URI.
    nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner;
    mBrowserDOMWindow->OpenURIInFrame(nsnull, nsnull,
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
    nsCString spec;
    aURI->GetSpec(spec);

    unused << SendLoadURL(spec);
}

void
TabParent::Show(const nsIntSize& size)
{
    // sigh
    unused << SendShow(size);
}

void
TabParent::UpdateDimensions(const nsRect& rect, const nsIntSize& size)
{
    unused << SendUpdateDimensions(rect, size);
}

void
TabParent::Activate()
{
    mActive = true;
    unused << SendActivate();
}

void
TabParent::Deactivate()
{
  mActive = false;
  unused << SendDeactivate();
}

bool
TabParent::Active()
{
  return mActive;
}

NS_IMETHODIMP
TabParent::Init(nsIDOMWindow *window)
{
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetState(PRUint32 *aState)
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
                                  const PRUint32& renderFlags,
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
TabParent::AllocPContentPermissionRequest(const nsCString& type, const IPC::URI& uri)
{
  return new ContentPermissionRequestParent(type, mFrameElement, uri);
}
  
bool
TabParent::DeallocPContentPermissionRequest(PContentPermissionRequestParent* actor)
{
  delete actor;
  return true;
}

void
TabParent::SendMouseEvent(const nsAString& aType, float aX, float aY,
                          PRInt32 aButton, PRInt32 aClickCount,
                          PRInt32 aModifiers, bool aIgnoreRootScrollFrame)
{
  unused << PBrowserParent::SendMouseEvent(nsString(aType), aX, aY,
                                           aButton, aClickCount,
                                           aModifiers, aIgnoreRootScrollFrame);
}

void
TabParent::SendKeyEvent(const nsAString& aType,
                        PRInt32 aKeyCode,
                        PRInt32 aCharCode,
                        PRInt32 aModifiers,
                        bool aPreventDefault)
{
  unused << PBrowserParent::SendKeyEvent(nsString(aType), aKeyCode, aCharCode,
                                         aModifiers, aPreventDefault);
}

bool TabParent::SendRealMouseEvent(nsMouseEvent& event)
{
  return PBrowserParent::SendRealMouseEvent(event);
}

bool TabParent::SendMouseScrollEvent(nsMouseScrollEvent& event)
{
  return PBrowserParent::SendMouseScrollEvent(event);
}

bool TabParent::SendRealKeyEvent(nsKeyEvent& event)
{
  return PBrowserParent::SendRealKeyEvent(event);
}

bool
TabParent::RecvSyncMessage(const nsString& aMessage,
                           const nsString& aJSON,
                           InfallibleTArray<nsString>* aJSONRetVal)
{
  return ReceiveMessage(aMessage, true, aJSON, aJSONRetVal);
}

bool
TabParent::RecvAsyncMessage(const nsString& aMessage,
                            const nsString& aJSON)
{
  return ReceiveMessage(aMessage, false, aJSON, nsnull);
}

bool
TabParent::RecvSetCursor(const PRUint32& aCursor)
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
  if (nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader()) {
    if (RenderFrameParent* frame = frameLoader->GetCurrentRemoteFrame()) {
      frame->SetBackgroundColor(aColor);
    }
  }
  return true;
}

bool
TabParent::RecvNotifyIMEFocus(const bool& aFocus,
                              nsIMEUpdatePreference* aPreference,
                              PRUint32* aSeqno)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

  *aSeqno = mIMESeqno;
  mIMETabParent = aFocus ? this : nsnull;
  mIMESelectionAnchor = 0;
  mIMESelectionFocus = 0;
  nsresult rv = widget->OnIMEFocusChange(aFocus);

  if (aFocus) {
    if (NS_SUCCEEDED(rv) && rv != NS_SUCCESS_IME_NO_UPDATES) {
      *aPreference = widget->GetIMEUpdatePreference();
    } else {
      aPreference->mWantUpdates = false;
      aPreference->mWantHints = false;
    }
  } else {
    mIMECacheText.Truncate(0);
  }
  return true;
}

bool
TabParent::RecvNotifyIMETextChange(const PRUint32& aStart,
                                   const PRUint32& aEnd,
                                   const PRUint32& aNewEnd)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

  widget->OnIMETextChange(aStart, aEnd, aNewEnd);
  return true;
}

bool
TabParent::RecvNotifyIMESelection(const PRUint32& aSeqno,
                                  const PRUint32& aAnchor,
                                  const PRUint32& aFocus)
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
        PRUint32 selLen = mIMESelectionAnchor > mIMESelectionFocus ?
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
      PRUint32 inputOffset = aEvent.mInput.mOffset,
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
  mIMESelectionAnchor = event.mOffset + (event.mReversed ? event.mLength : 0);
  mIMESelectionFocus = event.mOffset + (!event.mReversed ? event.mLength : 0);
  event.seqno = ++mIMESeqno;
  return PBrowserParent::SendSelectionEvent(event);
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
TabParent::RecvGetInputContext(PRInt32* aIMEEnabled,
                               PRInt32* aIMEOpen)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aIMEEnabled = IMEState::DISABLED;
    *aIMEOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    return true;
  }

  InputContext context = widget->GetInputContext();
  *aIMEEnabled = static_cast<PRInt32>(context.mIMEState.mEnabled);
  *aIMEOpen = static_cast<PRInt32>(context.mIMEState.mOpen);
  return true;
}

bool
TabParent::RecvSetInputContext(const PRInt32& aIMEEnabled,
                               const PRInt32& aIMEOpen,
                               const nsString& aType,
                               const nsString& aActionHint,
                               const PRInt32& aCause,
                               const PRInt32& aFocusChange)
{
  // mIMETabParent (which is actually static) tracks which if any TabParent has IMEFocus
  // When the input mode is set to anything but IMEState::DISABLED,
  // mIMETabParent should be set to this
  mIMETabParent =
    aIMEEnabled != static_cast<PRInt32>(IMEState::DISABLED) ? this : nsnull;
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget || !AllowContentIME())
    return true;

  InputContext context;
  context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(aIMEEnabled);
  context.mIMEState.mOpen = static_cast<IMEState::Open>(aIMEOpen);
  context.mHTMLInputType.Assign(aType);
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
  observerService->NotifyObservers(nsnull, "ime-enabled-state-changed", state.get());

  return true;
}

bool
TabParent::RecvGetDPI(float* aValue)
{
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
                          const nsString& aJSON,
                          InfallibleTArray<nsString>* aJSONRetVal)
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (frameLoader && frameLoader->GetFrameMessageManager()) {
    nsRefPtr<nsFrameMessageManager> manager =
      frameLoader->GetFrameMessageManager();
    JSContext* ctx = manager->GetJSContext();
    JSAutoRequest ar(ctx);
    PRUint32 len = 0; //TODO: obtain a real value in bug 572685
    // Because we want JS messages to have always the same properties,
    // create array even if len == 0.
    JSObject* objectsArray = JS_NewArrayObject(ctx, len, NULL);
    if (!objectsArray) {
      return false;
    }

    manager->ReceiveMessage(mFrameElement,
                            aMessage,
                            aSync,
                            aJSON,
                            objectsArray,
                            aJSONRetVal);
  }
  return true;
}

// nsIAuthPromptProvider

// This method is largely copied from nsDocShell::GetAuthPrompt
NS_IMETHODIMP
TabParent::GetAuthPrompt(PRUint32 aPromptReason, const nsIID& iid,
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
TabParent::AllocPContentDialog(const PRUint32& aType,
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
    PRUint32 index = mDelayedDialogs.Length() - 1;
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
      nsCAutoString url;
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
      InfallibleTArray<PRInt32> intParams;
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
TabParent::AllocPRenderFrame()
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  return new RenderFrameParent(frameLoader);
}

bool
TabParent::DeallocPRenderFrame(PRenderFrameParent* aFrame)
{
  delete aFrame;
  return true;
}

mozilla::docshell::POfflineCacheUpdateParent*
TabParent::AllocPOfflineCacheUpdate(const URI& aManifestURI,
                                    const URI& aDocumentURI,
                                    const nsCString& aClientID,
                                    const bool& stickDocument)
{
  nsRefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
    new mozilla::docshell::OfflineCacheUpdateParent();

  nsresult rv = update->Schedule(aManifestURI, aDocumentURI, aClientID,
                                 stickDocument);
  if (NS_FAILED(rv))
    return nsnull;

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
  return frameLoaderOwner ? frameLoaderOwner->GetFrameLoader() : nsnull;
}

already_AddRefed<nsIWidget>
TabParent::GetWidget() const
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (!content)
    return nsnull;

  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  return nsCOMPtr<nsIWidget>(frame->GetNearestWidget()).forget();
}

} // namespace tabs
} // namespace mozilla
