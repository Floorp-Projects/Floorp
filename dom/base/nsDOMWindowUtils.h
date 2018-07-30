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

class nsGlobalWindowOuter;
class nsIPresShell;
class nsIWidget;
class nsPresContext;
class nsIDocument;
class nsView;
struct nsPoint;

namespace mozilla {
  namespace dom {
    class Element;
  } // namespace dom
  namespace layers {
    class LayerTransactionChild;
    class WebRenderBridgeChild;
  } // namespace layers
} // namespace mozilla

class nsTranslationNodeList final : public nsITranslationNodeList
{
public:
  nsTranslationNodeList()
  {
    mNodes.SetCapacity(1000);
    mNodeIsRoot.SetCapacity(1000);
    mLength = 0;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSLATIONNODELIST

  void AppendElement(nsINode* aElement, bool aIsRoot)
  {
    mNodes.AppendElement(aElement);
    mNodeIsRoot.AppendElement(aIsRoot);
    mLength++;
  }

private:
  ~nsTranslationNodeList() {}

  nsTArray<nsCOMPtr<nsINode> > mNodes;
  nsTArray<bool> mNodeIsRoot;
  uint32_t mLength;
};

class nsDOMWindowUtils final : public nsIDOMWindowUtils,
                               public nsSupportsWeakReference
{
  typedef mozilla::widget::TextEventDispatcher
    TextEventDispatcher;
public:
  explicit nsDOMWindowUtils(nsGlobalWindowOuter *aWindow);
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

  nsIPresShell* GetPresShell();
  nsPresContext* GetPresContext();
  nsIDocument* GetDocument();
  mozilla::layers::LayerTransactionChild* GetLayerTransaction();
  mozilla::layers::WebRenderBridgeChild* GetWebRenderBridge();

  // Until callers are annotated.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD SendMouseEventCommon(const nsAString& aType,
                                  float aX,
                                  float aY,
                                  int32_t aButton,
                                  int32_t aClickCount,
                                  int32_t aModifiers,
                                  bool aIgnoreRootScrollFrame,
                                  float aPressure,
                                  unsigned short aInputSourceArg,
                                  uint32_t aIdentifier,
                                  bool aToWindow,
                                  bool *aPreventDefault,
                                  bool aIsDOMEventSynthesized,
                                  bool aIsWidgetEventSynthesized,
                                  int32_t aButtons);

  NS_IMETHOD SendTouchEventCommon(const nsAString& aType,
                                  uint32_t* aIdentifiers,
                                  int32_t* aXs,
                                  int32_t* aYs,
                                  uint32_t* aRxs,
                                  uint32_t* aRys,
                                  float* aRotationAngles,
                                  float* aForces,
                                  uint32_t aCount,
                                  int32_t aModifiers,
                                  bool aIgnoreRootScrollFrame,
                                  bool aToWindow,
                                  bool* aPreventDefault);
};

#endif
