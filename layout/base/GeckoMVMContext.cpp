/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoMVMContext.h"

#include "mozilla/DisplayPortUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/VisualViewport.h"
#include "nsCOMPtr.h"
#include "nsGlobalWindowInner.h"
#include "nsIDOMEventListener.h"
#include "nsIFrame.h"
#include "nsIObserverService.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"

namespace mozilla {

GeckoMVMContext::GeckoMVMContext(dom::Document* aDocument,
                                 PresShell* aPresShell)
    : mDocument(aDocument), mPresShell(aPresShell) {
  if (nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow()) {
    mEventTarget = window->GetChromeEventHandler();
  }
}

void GeckoMVMContext::AddEventListener(const nsAString& aType,
                                       nsIDOMEventListener* aListener,
                                       bool aUseCapture) {
  if (mEventTarget) {
    mEventTarget->AddEventListener(aType, aListener, aUseCapture);
  }
}

void GeckoMVMContext::RemoveEventListener(const nsAString& aType,
                                          nsIDOMEventListener* aListener,
                                          bool aUseCapture) {
  if (mEventTarget) {
    mEventTarget->RemoveEventListener(aType, aListener, aUseCapture);
  }
}

void GeckoMVMContext::AddObserver(nsIObserver* aObserver, const char* aTopic,
                                  bool aOwnsWeak) {
  if (nsCOMPtr<nsIObserverService> observerService =
          services::GetObserverService()) {
    observerService->AddObserver(aObserver, aTopic, aOwnsWeak);
  }
}

void GeckoMVMContext::RemoveObserver(nsIObserver* aObserver,
                                     const char* aTopic) {
  if (nsCOMPtr<nsIObserverService> observerService =
          services::GetObserverService()) {
    observerService->RemoveObserver(aObserver, aTopic);
  }
}

void GeckoMVMContext::Destroy() {
  mEventTarget = nullptr;
  mDocument = nullptr;
  mPresShell = nullptr;
}

nsViewportInfo GeckoMVMContext::GetViewportInfo(
    const ScreenIntSize& aDisplaySize) const {
  MOZ_ASSERT(mDocument);
  return mDocument->GetViewportInfo(aDisplaySize);
}

CSSToLayoutDeviceScale GeckoMVMContext::CSSToDevPixelScale() const {
  MOZ_ASSERT(mPresShell);
  return mPresShell->GetPresContext()->CSSToDevPixelScale();
}

float GeckoMVMContext::GetResolution() const {
  MOZ_ASSERT(mPresShell);
  return mPresShell->GetResolution();
}

bool GeckoMVMContext::SubjectMatchesDocument(nsISupports* aSubject) const {
  MOZ_ASSERT(mDocument);
  return SameCOMIdentity(aSubject, ToSupports(mDocument));
}

Maybe<CSSRect> GeckoMVMContext::CalculateScrollableRectForRSF() const {
  MOZ_ASSERT(mPresShell);
  if (nsIScrollableFrame* rootScrollableFrame =
          mPresShell->GetRootScrollFrameAsScrollable()) {
    return Some(
        CSSRect::FromAppUnits(nsLayoutUtils::CalculateScrollableRectForFrame(
            rootScrollableFrame, nullptr)));
  }
  return Nothing();
}

bool GeckoMVMContext::IsResolutionUpdatedByApz() const {
  MOZ_ASSERT(mPresShell);
  return mPresShell->IsResolutionUpdatedByApz();
}

LayoutDeviceMargin
GeckoMVMContext::ScrollbarAreaToExcludeFromCompositionBounds() const {
  MOZ_ASSERT(mPresShell);
  return LayoutDeviceMargin::FromAppUnits(
      nsLayoutUtils::ScrollbarAreaToExcludeFromCompositionBoundsFor(
          mPresShell->GetRootScrollFrame()),
      mPresShell->GetPresContext()->AppUnitsPerDevPixel());
}

Maybe<LayoutDeviceIntSize> GeckoMVMContext::GetContentViewerSize() const {
  MOZ_ASSERT(mPresShell);
  LayoutDeviceIntSize result;
  if (nsLayoutUtils::GetContentViewerSize(mPresShell->GetPresContext(),
                                          result)) {
    return Some(result);
  }
  return Nothing();
}

bool GeckoMVMContext::AllowZoomingForDocument() const {
  MOZ_ASSERT(mDocument);
  return nsLayoutUtils::AllowZoomingForDocument(mDocument);
}

bool GeckoMVMContext::IsInReaderMode() const {
  MOZ_ASSERT(mDocument);
  nsString uri;
  if (NS_FAILED(mDocument->GetDocumentURI(uri))) {
    return false;
  }
  static auto readerModeUriPrefix = u"about:reader"_ns;
  return StringBeginsWith(uri, readerModeUriPrefix);
}

bool GeckoMVMContext::IsDocumentLoading() const {
  MOZ_ASSERT(mDocument);
  return mDocument->GetReadyStateEnum() == dom::Document::READYSTATE_LOADING;
}

void GeckoMVMContext::SetResolutionAndScaleTo(float aResolution,
                                              ResolutionChangeOrigin aOrigin) {
  MOZ_ASSERT(mPresShell);
  mPresShell->SetResolutionAndScaleTo(aResolution, aOrigin);
}

void GeckoMVMContext::SetVisualViewportSize(const CSSSize& aSize) {
  MOZ_ASSERT(mPresShell);
  mPresShell->SetVisualViewportSize(
      nsPresContext::CSSPixelsToAppUnits(aSize.width),
      nsPresContext::CSSPixelsToAppUnits(aSize.height));
}

void GeckoMVMContext::PostVisualViewportResizeEventByDynamicToolbar() {
  MOZ_ASSERT(mDocument);

  // We only fire visual viewport events and don't want to cause any explicit
  // reflows here since in general we don't use the up-to-date visual viewport
  // size for layout.
  if (auto* window = nsGlobalWindowInner::Cast(mDocument->GetInnerWindow())) {
    window->VisualViewport()->PostResizeEvent();
  }
}

void GeckoMVMContext::UpdateDisplayPortMargins() {
  MOZ_ASSERT(mPresShell);
  if (nsIFrame* root = mPresShell->GetRootScrollFrame()) {
    nsIContent* content = root->GetContent();
    bool hasDisplayPort = DisplayPortUtils::HasNonMinimalDisplayPort(content);
    bool hasResolution = mPresShell->GetResolution() != 1.0f;
    if (!hasDisplayPort && !hasResolution) {
      // We only want to update the displayport if there is one already, or
      // add one if there's a resolution on the document (see bug 1225508
      // comment 1).
      return;
    }
    nsRect displayportBase = nsRect(
        nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(root));
    // We only create MobileViewportManager for root content documents. If that
    // ever changes we'd need to limit the size of this displayport base rect
    // because non-toplevel documents have no limit on their size.
    MOZ_ASSERT(
        mPresShell->GetPresContext()->IsRootContentDocumentCrossProcess());
    DisplayPortUtils::SetDisplayPortBaseIfNotSet(content, displayportBase);
    nsIScrollableFrame* scrollable = do_QueryFrame(root);
    DisplayPortUtils::CalculateAndSetDisplayPortMargins(
        scrollable, DisplayPortUtils::RepaintMode::Repaint);
  }
}

void GeckoMVMContext::Reflow(const CSSSize& aNewSize) {
  RefPtr doc = mDocument;
  RefPtr ps = mPresShell;

  MOZ_ASSERT(doc);
  MOZ_ASSERT(ps);

  if (ps->ResizeReflowIgnoreOverride(CSSPixel::ToAppUnits(aNewSize.width),
                                     CSSPixel::ToAppUnits(aNewSize.height))) {
    doc->FlushPendingNotifications(FlushType::InterruptibleLayout);
  }
}

}  // namespace mozilla
