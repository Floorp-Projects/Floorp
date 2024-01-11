/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MVMContext_h_
#define MVMContext_h_

#include "Units.h"
#include "mozilla/Maybe.h"
#include "mozilla/PresShellForwards.h"
#include "nsISupportsImpl.h"
#include "nsStringFwd.h"
#include "nsViewportInfo.h"

class nsIDOMEventListener;
class nsIObserver;
class nsISupports;

namespace mozilla {

/**
 * The interface MobileViewportManager uses to interface with its surroundings.
 * This mainly exists to facilitate testing MobileViewportManager in isolation
 * from the rest of Gecko.
 */
class MVMContext {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MVMContext)
 protected:
  virtual ~MVMContext() {}

 public:
  virtual void AddEventListener(const nsAString& aType,
                                nsIDOMEventListener* aListener,
                                bool aUseCapture) = 0;
  virtual void RemoveEventListener(const nsAString& aType,
                                   nsIDOMEventListener* aListener,
                                   bool aUseCapture) = 0;
  virtual void AddObserver(nsIObserver* aObserver, const char* aTopic,
                           bool aOwnsWeak) = 0;
  virtual void RemoveObserver(nsIObserver* aObserver, const char* aTopic) = 0;
  virtual void Destroy() = 0;

  virtual nsViewportInfo GetViewportInfo(
      const ScreenIntSize& aDisplaySize) const = 0;
  virtual CSSToLayoutDeviceScale CSSToDevPixelScale() const = 0;
  virtual float GetResolution() const = 0;
  virtual bool SubjectMatchesDocument(nsISupports* aSubject) const = 0;
  virtual Maybe<CSSRect> CalculateScrollableRectForRSF() const = 0;
  virtual bool IsResolutionUpdatedByApz() const = 0;
  virtual LayoutDeviceMargin ScrollbarAreaToExcludeFromCompositionBounds()
      const = 0;
  virtual Maybe<LayoutDeviceIntSize> GetDocumentViewerSize() const = 0;
  virtual bool AllowZoomingForDocument() const = 0;
  virtual bool IsInReaderMode() const = 0;
  virtual bool IsDocumentLoading() const = 0;

  virtual void SetResolutionAndScaleTo(float aResolution,
                                       ResolutionChangeOrigin aOrigin) = 0;
  virtual void SetVisualViewportSize(const CSSSize& aSize) = 0;
  virtual void PostVisualViewportResizeEventByDynamicToolbar() = 0;
  virtual void UpdateDisplayPortMargins() = 0;

  virtual void Reflow(const CSSSize& aNewSize) = 0;
  virtual ScreenIntCoord GetDynamicToolbarOffset() = 0;
};

}  // namespace mozilla

#endif  // MVMContext_h_
