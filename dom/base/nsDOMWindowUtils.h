/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMWindowUtils_h_
#define nsDOMWindowUtils_h_

#include "nsWeakReference.h"

#include "nsIDOMWindowUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Result.h"

class nsGlobalWindowOuter;
class nsIDocShell;
class nsIWidget;
class nsPresContext;
class nsView;
struct nsPoint;

namespace mozilla {
class PresShell;
namespace dom {
class Document;
class Element;
}  // namespace dom
namespace layers {
class LayerTransactionChild;
class WebRenderBridgeChild;
}  // namespace layers
}  // namespace mozilla

class nsTranslationNodeList final : public nsITranslationNodeList {
 public:
  nsTranslationNodeList() {
    mNodes.SetCapacity(1000);
    mNodeIsRoot.SetCapacity(1000);
    mLength = 0;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSLATIONNODELIST

  void AppendElement(nsINode* aElement, bool aIsRoot) {
    mNodes.AppendElement(aElement);
    mNodeIsRoot.AppendElement(aIsRoot);
    mLength++;
  }

 private:
  ~nsTranslationNodeList() = default;

  nsTArray<nsCOMPtr<nsINode> > mNodes;
  nsTArray<bool> mNodeIsRoot;
  uint32_t mLength;
};

class nsDOMWindowUtils final : public nsIDOMWindowUtils,
                               public nsSupportsWeakReference {
  using TextEventDispatcher = mozilla::widget::TextEventDispatcher;

 public:
  explicit nsDOMWindowUtils(nsGlobalWindowOuter* aWindow);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWINDOWUTILS

 protected:
  ~nsDOMWindowUtils();

  nsWeakPtr mWindow;

  // If aOffset is non-null, it gets filled in with the offset of the root
  // frame of our window to the nearest widget in the app units of our window.
  // Add this offset to any event offset we're given to make it relative to the
  // widget returned by GetWidget.
  nsIWidget* GetWidget(nsPoint* aOffset = nullptr);
  nsIWidget* GetWidgetForElement(mozilla::dom::Element* aElement);

  nsIDocShell* GetDocShell();
  mozilla::PresShell* GetPresShell();
  nsPresContext* GetPresContext();
  mozilla::dom::Document* GetDocument();
  mozilla::layers::LayerTransactionChild* GetLayerTransaction();
  mozilla::layers::WebRenderBridgeChild* GetWebRenderBridge();
  mozilla::layers::CompositorBridgeChild* GetCompositorBridge();

  // Until callers are annotated.
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD SendMouseEventCommon(
      const nsAString& aType, float aX, float aY, int32_t aButton,
      int32_t aClickCount, int32_t aModifiers, bool aIgnoreRootScrollFrame,
      float aPressure, unsigned short aInputSourceArg, uint32_t aIdentifier,
      bool aToWindow, bool* aPreventDefault, bool aIsDOMEventSynthesized,
      bool aIsWidgetEventSynthesized, int32_t aButtons);

  MOZ_CAN_RUN_SCRIPT
  nsresult SendTouchEventCommon(
      const nsAString& aType, const nsTArray<uint32_t>& aIdentifiers,
      const nsTArray<int32_t>& aXs, const nsTArray<int32_t>& aYs,
      const nsTArray<uint32_t>& aRxs, const nsTArray<uint32_t>& aRys,
      const nsTArray<float>& aRotationAngles, const nsTArray<float>& aForces,
      int32_t aModifiers, bool aIgnoreRootScrollFrame, bool aToWindow,
      bool* aPreventDefault);

  void ReportErrorMessageForWindow(const nsAString& aErrorMessage,
                                   const char* aClassification,
                                   bool aFromChrome);

 private:
  mozilla::Result<mozilla::ScreenRect, nsresult> ConvertToScreenRect(
      float aX, float aY, float aWidth, float aHeight);
};

#endif
