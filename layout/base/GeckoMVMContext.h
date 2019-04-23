/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoMVMContext_h_
#define GeckoMVMContext_h_

#include "MVMContext.h"

#include "mozilla/Attributes.h"  // for MOZ_NON_OWNING_REF
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"

namespace mozilla {
class PresShell;
namespace dom {
class Document;
class EventTarget;
}  // namespace dom

/**
 * An implementation of MVMContext that uses actual Gecko components.
 * This is intended for production use (whereas TestMVMContext is intended for
 * testing.)
 */
class GeckoMVMContext : public MVMContext {
 public:
  explicit GeckoMVMContext(dom::Document* aDocument, PresShell* aPresShell);
  void AddEventListener(const nsAString& aType, nsIDOMEventListener* aListener,
                        bool aUseCapture) override;
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener,
                           bool aUseCapture) override;
  void AddObserver(nsIObserver* aObserver, const char* aTopic,
                   bool aOwnsWeak) override;
  void RemoveObserver(nsIObserver* aObserver, const char* aTopic) override;
  void Destroy() override;

  nsViewportInfo GetViewportInfo(
      const ScreenIntSize& aDisplaySize) const override;
  CSSToLayoutDeviceScale CSSToDevPixelScale() const override;
  float GetResolution() const override;
  bool SubjectMatchesDocument(nsISupports* aSubject) const override;
  Maybe<CSSRect> CalculateScrollableRectForRSF() const override;
  bool IsResolutionUpdatedByApz() const override;
  LayoutDeviceMargin ScrollbarAreaToExcludeFromCompositionBounds()
      const override;
  Maybe<LayoutDeviceIntSize> GetContentViewerSize() const override;
  bool AllowZoomingForDocument() const override;

  void SetResolutionAndScaleTo(float aResolution) override;
  void SetVisualViewportSize(const CSSSize& aSize) override;
  void UpdateDisplayPortMargins() override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void Reflow(const CSSSize& aNewSize, const CSSSize& aOldSize,
              ResizeEventFlag aResizeEventFlag) override;

 private:
  RefPtr<dom::Document> mDocument;
  // raw ref since the presShell owns this
  PresShell* MOZ_NON_OWNING_REF mPresShell;
  nsCOMPtr<dom::EventTarget> mEventTarget;
};

}  // namespace mozilla

#endif  // GeckoMVMContext_h_
