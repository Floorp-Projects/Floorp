/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ACCESSIBILITY) && defined(XP_WIN)
#  include "mozilla/a11y/ProxyAccessible.h"
#  include "mozilla/a11y/ProxyWrappers.h"
#endif
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MozFrameLoaderOwnerBinding.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsIDocShellTreeOwner.h"
#include "nsQueryObject.h"
#include "nsSubDocumentFrame.h"
#include "nsView.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

BrowserBridgeChild::BrowserBridgeChild(nsFrameLoader* aFrameLoader,
                                       BrowsingContext* aBrowsingContext,
                                       TabId aId)
    : mId{aId},
      mLayersId{0},
      mIPCOpen(true),
      mFrameLoader(aFrameLoader),
      mBrowsingContext(aBrowsingContext) {}

BrowserBridgeChild::~BrowserBridgeChild() {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  if (mEmbeddedDocAccessible) {
    mEmbeddedDocAccessible->Shutdown();
  }
#endif
}

void BrowserBridgeChild::NavigateByKey(bool aForward,
                                       bool aForDocumentNavigation) {
  Unused << SendNavigateByKey(aForward, aForDocumentNavigation);
}

void BrowserBridgeChild::Activate() { Unused << SendActivate(); }

void BrowserBridgeChild::Deactivate(bool aWindowLowering) {
  Unused << SendDeactivate(aWindowLowering);
}

void BrowserBridgeChild::SetIsUnderHiddenEmbedderElement(
    bool aIsUnderHiddenEmbedderElement) {
  Unused << SendSetIsUnderHiddenEmbedderElement(aIsUnderHiddenEmbedderElement);
}

/*static*/
BrowserBridgeChild* BrowserBridgeChild::GetFrom(nsFrameLoader* aFrameLoader) {
  if (!aFrameLoader) {
    return nullptr;
  }
  return aFrameLoader->GetBrowserBridgeChild();
}

/*static*/
BrowserBridgeChild* BrowserBridgeChild::GetFrom(nsIContent* aContent) {
  RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(aContent);
  if (!loaderOwner) {
    return nullptr;
  }
  RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  return GetFrom(frameLoader);
}

IPCResult BrowserBridgeChild::RecvSetLayersId(
    const mozilla::layers::LayersId& aLayersId) {
  MOZ_ASSERT(!mLayersId.IsValid() && aLayersId.IsValid());
  mLayersId = aLayersId;

  // Invalidate the nsSubdocumentFrame now that we have a layers ID for the
  // child browser
  if (RefPtr<Element> owner = mFrameLoader->GetOwnerContent()) {
    if (nsIFrame* frame = owner->GetPrimaryFrame()) {
      frame->InvalidateFrame();
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvRequestFocus(
    const bool& aCanRaise) {
  // Adapted from BrowserParent
  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return IPC_OK();
  }
  nsContentUtils::RequestFrameFocus(*owner, aCanRaise);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvMoveFocus(
    const bool& aForward, const bool& aForDocumentNavigation) {
  // Adapted from BrowserParent
  nsCOMPtr<nsIFocusManager> fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return IPC_OK();
  }

  RefPtr<Element> dummy;

  uint32_t type =
      aForward
          ? (aForDocumentNavigation
                 ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARDDOC)
                 : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARD))
          : (aForDocumentNavigation
                 ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARDDOC)
                 : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARD));
  fm->MoveFocus(nullptr, owner, type, nsIFocusManager::FLAG_BYKEY,
                getter_AddRefs(dummy));
  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserBridgeChild::RecvSetEmbeddedDocAccessibleCOMProxy(
    const a11y::IDispatchHolder& aCOMProxy) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  MOZ_ASSERT(!aCOMProxy.IsNull());
  if (mEmbeddedDocAccessible) {
    mEmbeddedDocAccessible->Shutdown();
  }
  RefPtr<IDispatch> comProxy(aCOMProxy.Get());
  mEmbeddedDocAccessible =
      new a11y::RemoteIframeDocProxyAccessibleWrap(comProxy);
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvFireFrameLoadEvent(
    bool aIsTrusted) {
  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return IPC_OK();
  }

  // Fire the `load` event on our embedder element.
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetEvent event(aIsTrusted, eLoad);
  event.mFlags.mBubbles = false;
  event.mFlags.mCancelable = false;
  EventDispatcher::Dispatch(owner, nullptr, &event, nullptr, &status);

  UnblockOwnerDocsLoadEvent(owner->OwnerDoc());

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvScrollRectIntoView(
    const nsRect& aRect, const ScrollAxis& aVertical,
    const ScrollAxis& aHorizontal, const ScrollFlags& aScrollFlags,
    const int32_t& aAppUnitsPerDevPixel) {
  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return IPC_OK();
  }

  nsIFrame* frame = owner->GetPrimaryFrame();
  if (!frame) {
    return IPC_OK();
  }

  nsSubDocumentFrame* subdocumentFrame = do_QueryFrame(frame);
  if (!subdocumentFrame) {
    return IPC_OK();
  }

  nsPoint extraOffset = subdocumentFrame->GetExtraOffset();

  int32_t parentAPD = frame->PresContext()->AppUnitsPerDevPixel();
  nsRect rect =
      aRect.ScaleToOtherAppUnitsRoundOut(aAppUnitsPerDevPixel, parentAPD);
  rect += extraOffset;
  RefPtr<PresShell> presShell = frame->PresShell();
  presShell->ScrollFrameRectIntoView(frame, rect, aVertical, aHorizontal,
                                     aScrollFlags);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvSubFrameCrashed(
    BrowsingContext* aContext) {
  if (aContext) {
    RefPtr<nsFrameLoaderOwner> frameLoaderOwner =
        do_QueryObject(aContext->GetEmbedderElement());
    IgnoredErrorResult rv;
    RemotenessOptions options;
    options.mError.Construct(static_cast<uint32_t>(NS_ERROR_FRAME_CRASHED));
    frameLoaderOwner->ChangeRemoteness(options, rv);

    if (NS_WARN_IF(rv.Failed())) {
      return IPC_FAIL(this, "Remoteness change failed");
    }
  }

  return IPC_OK();
}

void BrowserBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;

  // Ensure we unblock our document's 'load' event (in case the OOP-iframe has
  // been removed before it finishes loading, or its subprocess crashed):
  if (RefPtr<Element> owner = mFrameLoader->GetOwnerContent()) {
    UnblockOwnerDocsLoadEvent(owner->OwnerDoc());
  }
}

void BrowserBridgeChild::UnblockOwnerDocsLoadEvent(Document* aOwnerDoc) {
  // XXX bug 1576296: Is it expected that we sometimes don't have a docShell?
  if (!mHadInitialLoad && aOwnerDoc->GetDocShell()) {
    mHadInitialLoad = true;
    nsDocShell::Cast(aOwnerDoc->GetDocShell())->OOPChildLoadDone(this);
  }
}

}  // namespace dom
}  // namespace mozilla
