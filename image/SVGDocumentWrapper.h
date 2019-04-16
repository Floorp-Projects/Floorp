/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This class wraps an SVG document, for use by VectorImage objects. */

#ifndef mozilla_image_SVGDocumentWrapper_h
#define mozilla_image_SVGDocumentWrapper_h

#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIObserver.h"
#include "nsIContentViewer.h"
#include "nsWeakReference.h"
#include "nsSize.h"

class nsIRequest;
class nsILoadGroup;
class nsIFrame;

#define OBSERVER_SVC_CID "@mozilla.org/observer-service;1"

namespace mozilla {
class PresShell;
namespace dom {
class SVGSVGElement;
class SVGDocument;
}  // namespace dom

namespace image {

class SVGDocumentWrapper final : public nsIStreamListener,
                                 public nsIObserver,
                                 nsSupportsWeakReference {
 public:
  SVGDocumentWrapper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIOBSERVER

  enum Dimension { eWidth, eHeight };

  /**
   * Returns the wrapped document, or nullptr on failure. (No AddRef.)
   */
  mozilla::dom::SVGDocument* GetDocument();

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
   * Returns the mozilla::PresShell for the wrapped document.
   */
  inline mozilla::PresShell* GetPresShell() { return mViewer->GetPresShell(); }

  /**
   * Modifier to update the viewport dimensions of the wrapped document. This
   * method performs a synchronous "FlushType::Layout" on the wrapped document,
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
  bool IsAnimated();

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
  float GetCurrentTimeAsFloat();
  void SetCurrentTime(float aTime);
  void TickRefreshDriver();

  /**
   * Force a layout flush of the underlying SVG document.
   */
  void FlushLayout();

 private:
  ~SVGDocumentWrapper();

  nsresult SetupViewer(nsIRequest* aRequest, nsIContentViewer** aViewer,
                       nsILoadGroup** aLoadGroup);
  void DestroyViewer();
  void RegisterForXPCOMShutdown();
  void UnregisterForXPCOMShutdown();

  nsCOMPtr<nsIContentViewer> mViewer;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIStreamListener> mListener;
  bool mIgnoreInvalidation;
  bool mRegisteredForXPCOMShutdown;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_SVGDocumentWrapper_h
