/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This class wraps an SVG document, for use by VectorImage objects. */

#ifndef mozilla_imagelib_SVGDocumentWrapper_h_
#define mozilla_imagelib_SVGDocumentWrapper_h_

#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIObserver.h"
#include "nsIContentViewer.h"
#include "nsWeakReference.h"

class nsIAtom;
class nsIPresShell;
class nsIRequest;
class nsILoadGroup;
class nsIFrame;
struct nsIntSize;

#define OBSERVER_SVC_CID "@mozilla.org/observer-service;1"

// undef the GetCurrentTime macro defined in WinBase.h from the MS Platform SDK
#undef GetCurrentTime

namespace mozilla {
namespace dom {
class SVGSVGElement;
}

namespace image {

class SVGDocumentWrapper MOZ_FINAL : public nsIStreamListener,
                                     public nsIObserver,
                                     nsSupportsWeakReference
{
public:
  SVGDocumentWrapper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIOBSERVER

  enum Dimension {
    eWidth,
    eHeight
  };

  /**
   * Looks up the value of the wrapped SVG document's |width| or |height|
   * attribute in CSS pixels, and returns it by reference.  If the document has
   * a percent value for the queried attribute, then this method fails
   * (returns false).
   *
   * @param aDimension    Indicates whether the width or height is desired.
   * @param[out] aResult  If this method succeeds, then this outparam will be
                          populated with the width or height in CSS pixels.
   * @return false to indicate failure, if the queried attribute has a
   *         percent value.  Otherwise, true.
   *
   */
  bool      GetWidthOrHeight(Dimension aDimension, int32_t& aResult);

  /**
   * Returns the wrapped document, or nullptr on failure. (No AddRef.)
   */
  nsIDocument* GetDocument();

  /**
   * Returns the root <svg> element for the wrapped document, or nullptr on
   * failure.
   */
  mozilla::dom::SVGSVGElement* GetRootSVGElem();

  /**
   * Returns the root nsIFrame* for the wrapped document, or nullptr on failure.
   *
   * @return the root nsIFrame* for the wrapped document, or nullptr on failure.
   */
  nsIFrame* GetRootLayoutFrame();

  /**
   * Returns (by reference) the nsIPresShell for the wrapped document.
   *
   * @param[out] aPresShell On success, this will be populated with a pointer
   *                        to the wrapped document's nsIPresShell.
   *
   * @return NS_OK on success, or an error code on failure.
   */
  inline nsresult  GetPresShell(nsIPresShell** aPresShell)
    { return mViewer->GetPresShell(aPresShell); }

  /**
   * Modifier to update the viewport dimensions of the wrapped document. This
   * method performs a synchronous "Flush_Layout" on the wrapped document,
   * since a viewport-change affects layout.
   *
   * @param aViewportSize The new viewport dimensions.
   */
  void UpdateViewportBounds(const nsIntSize& aViewportSize);

  /**
   * If an SVG image's helper document has a pending notification for an
   * override on the root node's "preserveAspectRatio" attribute, then this
   * method will flush that notification so that the image can paint correctly.
   * (First, though, it sets the mIgnoreInvalidation flag so that we won't
   * notify the image's observers and trigger unwanted repaint-requests.)
   */
  void FlushImageTransformInvalidation();

  /**
   * Returns a bool indicating whether the document has any SMIL animations.
   *
   * @return true if the document has any SMIL animations. Else, false.
   */
  bool      IsAnimated();

  /**
   * Indicates whether we should currently ignore rendering invalidations sent
   * from the wrapped SVG doc.
   *
   * @return true if we should ignore invalidations sent from this SVG doc.
   */
  bool ShouldIgnoreInvalidation() { return mIgnoreInvalidation; }

  /**
   * Methods to control animation.
   */
  void StartAnimation();
  void StopAnimation();
  void ResetAnimation();
  float GetCurrentTime();
  void SetCurrentTime(float aTime);

  /**
   * Force a layout flush of the underlying SVG document.
   */
  void FlushLayout();

private:
  ~SVGDocumentWrapper();

  nsresult SetupViewer(nsIRequest *aRequest,
                       nsIContentViewer** aViewer,
                       nsILoadGroup** aLoadGroup);
  void     DestroyViewer();
  void     RegisterForXPCOMShutdown();
  void     UnregisterForXPCOMShutdown();

  nsCOMPtr<nsIContentViewer>  mViewer;
  nsCOMPtr<nsILoadGroup>      mLoadGroup;
  nsCOMPtr<nsIStreamListener> mListener;
  bool                        mIgnoreInvalidation;
  bool                        mRegisteredForXPCOMShutdown;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_imagelib_SVGDocumentWrapper_h_
