/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGOBSERVERUTILS_H_
#define LAYOUT_SVG_SVGOBSERVERUTILS_H_

#include "mozilla/Attributes.h"
#include "mozilla/SVGIntegrationUtils.h"
#include "mozilla/dom/IDTracker.h"
#include "FrameProperties.h"
#include "nsID.h"
#include "nsIFrame.h"  // only for LayoutFrameType
#include "nsIMutationObserver.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsIReferrerInfo.h"
#include "nsStringFwd.h"
#include "nsStubMutationObserver.h"
#include "nsStyleStruct.h"

class nsAtom;
class nsCycleCollectionTraversalCallback;
class nsIFrame;
class nsIURI;

namespace mozilla {
class SVGClipPathFrame;
class SVGFilterFrame;
class SVGMarkerFrame;
class SVGMaskFrame;
class SVGPaintServerFrame;

namespace dom {
class CanvasRenderingContext2D;
class Element;
class SVGGeometryElement;
class SVGMPathElement;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {

/*
 * This class contains URL and referrer information (referrer and referrer
 * policy).
 * We use it to pass to svg system instead of nsIURI. The object brings referrer
 * and referrer policy so we can send correct Referer headers.
 */
class URLAndReferrerInfo {
 public:
  URLAndReferrerInfo(nsIURI* aURI, nsIReferrerInfo* aReferrerInfo)
      : mURI(aURI), mReferrerInfo(aReferrerInfo) {
    MOZ_ASSERT(aURI);
  }

  URLAndReferrerInfo(nsIURI* aURI, const URLExtraData& aExtraData)
      : mURI(aURI), mReferrerInfo(aExtraData.ReferrerInfo()) {
    MOZ_ASSERT(aURI);
  }

  NS_INLINE_DECL_REFCOUNTING(URLAndReferrerInfo)

  nsIURI* GetURI() const { return mURI; }
  nsIReferrerInfo* GetReferrerInfo() const { return mReferrerInfo; }

  bool operator==(const URLAndReferrerInfo& aRHS) const;

 private:
  ~URLAndReferrerInfo() = default;

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
};

/**
 * This interface allows us to be notified when a piece of SVG content is
 * re-rendered.
 *
 * Concrete implementations of this base class need to implement
 * GetReferencedElementWithoutObserving to specify the SVG element that
 * they'd like to monitor for rendering changes, and they need to implement
 * OnRenderingChange to specify how we'll react when that content gets
 * re-rendered.  They also need to implement a constructor and destructor,
 * which should call StartObserving and StopObserving, respectively.
 *
 * The referenced element is generally looked up and stored during
 * construction.  If the referenced element is in an extenal SVG resource
 * document, the lookup code will initiate loading of the external resource and
 * OnRenderingChange will be called once the element in the external resource
 * is available.
 *
 * Although the referenced element may be found and stored during construction,
 * observing for rendering changes does not start until requested.
 */
class SVGRenderingObserver : public nsStubMutationObserver {
 protected:
  virtual ~SVGRenderingObserver() = default;

 public:
  enum Flags : uint32_t {
    OBSERVE_ATTRIBUTE_CHANGES = 0x01,
    OBSERVE_CONTENT_CHANGES = 0x02
  };
  using Element = dom::Element;

  SVGRenderingObserver(uint32_t aFlags = OBSERVE_ATTRIBUTE_CHANGES |
                                         OBSERVE_CONTENT_CHANGES)
      : mInObserverSet(false), mFlags(aFlags) {}

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  /**
   * Called when non-DOM-mutation changes to the observed element should likely
   * cause the rendering of our observer to change.  This includes changes to
   * CSS computed values, but also changes to rendering observers that the
   * observed element itself may have (for example, when we're being used to
   * observe an SVG pattern, and an element in that pattern references and
   * observes a gradient that has changed).
   */
  void OnNonDOMMutationRenderingChange();

  // When a SVGRenderingObserver list gets forcibly cleared, it uses this
  // callback to notify every observer that's cleared from it, so they can
  // react.
  void NotifyEvictedFromRenderingObserverSet();

  nsIFrame* GetAndObserveReferencedFrame();
  /**
   * @param aOK this is only for the convenience of callers. We set *aOK to
   * false if the frame is the wrong type
   */
  nsIFrame* GetAndObserveReferencedFrame(mozilla::LayoutFrameType aFrameType,
                                         bool* aOK);

  Element* GetAndObserveReferencedElement();

  virtual bool ObservesReflow() { return false; }

 protected:
  void StartObserving();
  void StopObserving();

  /**
   * Called whenever the rendering of the observed element may have changed.
   *
   * More specifically, this method is called whenever DOM mutation occurs in
   * the observed element's subtree, or whenever
   * SVGObserverUtils::InvalidateRenderingObservers or
   * SVGObserverUtils::InvalidateDirectRenderingObservers is called for the
   * observed element's frame.
   *
   * Subclasses should override this method to handle rendering changes
   * appropriately.
   */
  virtual void OnRenderingChange() = 0;

  virtual Element* GetReferencedElementWithoutObserving() = 0;

#ifdef DEBUG
  void DebugObserverSet();
#endif

  // Whether we're in our observed element's observer set at this time.
  bool mInObserverSet;

  // Flags to control what changes we notify about.
  uint32_t mFlags;
};

class SVGObserverUtils {
 public:
  using CanvasRenderingContext2D = dom::CanvasRenderingContext2D;
  using Element = dom::Element;
  using SVGGeometryElement = dom::SVGGeometryElement;
  using HrefToTemplateCallback = const std::function<void(nsAString&)>&;

  /**
   * Ensures that that if the given frame requires any resources that are in
   * SVG resource documents that the loading of those documents is initiated.
   * This does not make aFrame start to observe any elements that it
   * references.
   */
  static void InitiateResourceDocLoads(nsIFrame* aFrame);

  /**
   * Called when changes to an element (e.g. CSS property changes) cause its
   * frame to start/stop referencing (or reference different) SVG resource
   * elements. (_Not_ called for changes to referenced resource elements.)
   *
   * This function handles such changes by discarding _all_ the frame's SVG
   * effects frame properties (causing those properties to stop watching their
   * target element). It also synchronously (re)creates the filter and marker
   * frame properties (XXX why not the other properties?), which makes it
   * useful for initializing those properties during first reflow.
   *
   * XXX rename to something more meaningful like RefreshResourceReferences?
   */
  static void UpdateEffects(nsIFrame* aFrame);

  /**
   * @param aFrame must be a first-continuation.
   */
  static void AddRenderingObserver(Element* aElement,
                                   SVGRenderingObserver* aObserver);
  /**
   * @param aFrame must be a first-continuation.
   */
  static void RemoveRenderingObserver(Element* aElement,
                                      SVGRenderingObserver* aObserver);

  /**
   * Removes all rendering observers from aElement.
   */
  static void RemoveAllRenderingObservers(Element* aElement);

  /**
   * This can be called on any frame. We invalidate the observers of aFrame's
   * element, if any, or else walk up to the nearest observable SVG parent
   * frame with observers and invalidate them instead.
   *
   * Note that this method is very different to e.g.
   * MutationObservers::AttributeChanged which walks up the content node tree
   * all the way to the root node (not stopping if it encounters a non-container
   * SVG node) invalidating all mutation observers (not just
   * SVGRenderingObservers) on all nodes along the way (not just the first
   * node it finds with observers). In other words, by doing all the
   * things in parentheses in the preceding sentence, this method uses
   * knowledge about our implementation and what can be affected by SVG effects
   * to make invalidation relatively lightweight when an SVG effect changes.
   */
  static void InvalidateRenderingObservers(nsIFrame* aFrame);

  enum { INVALIDATE_REFLOW = 1 };

  enum ReferenceState {
    /// Has no references to SVG filters (may still have CSS filter functions!)
    eHasNoRefs,
    eHasRefsAllValid,
    eHasRefsSomeInvalid,
  };

  /**
   * This can be called on any element or frame. Only direct observers of this
   * (frame's) element, if any, are invalidated.
   */
  static void InvalidateDirectRenderingObservers(Element* aElement,
                                                 uint32_t aFlags = 0);
  static void InvalidateDirectRenderingObservers(nsIFrame* aFrame,
                                                 uint32_t aFlags = 0);

  /**
   * Get the paint server for aPaintedFrame.
   */
  static SVGPaintServerFrame* GetAndObservePaintServer(
      nsIFrame* aPaintedFrame, mozilla::StyleSVGPaint nsStyleSVG::*aPaint);

  /**
   * Get the start/mid/end-markers for the given frame, and add the frame as
   * an observer to those markers.  Returns true if at least one marker type is
   * found, false otherwise.
   */
  static bool GetAndObserveMarkers(nsIFrame* aMarkedFrame,
                                   SVGMarkerFrame* (*aFrames)[3]);

  /**
   * Get the frames of the SVG filters applied to the given frame, and add the
   * frame as an observer to those filter frames.
   *
   * NOTE! A return value of eHasNoRefs does NOT mean that there are no filters
   * to be applied, only that there are no references to SVG filter elements.
   *
   * @param aIsBackdrop whether we're observing a backdrop-filter or a filter.
   *
   * XXX Callers other than ComputePostEffectsInkOverflowRect and
   * SVGUtils::GetPostFilterInkOverflowRect should not need to initiate
   * observing.  If we have a bug that causes invalidation (which would remove
   * observers) between reflow and painting, then we don't really want to
   * re-add abservers during painting.  That has the potential to hide logic
   * bugs, or cause later invalidation problems.  However, let's not change
   * that behavior just yet due to the regression potential.
   */
  static ReferenceState GetAndObserveFilters(
      nsIFrame* aFilteredFrame, nsTArray<SVGFilterFrame*>* aFilterFrames,
      StyleFilterType aStyleFilterType = StyleFilterType::Filter);

  /*
   * NOTE! canvas doesn't have backdrop-filters so there's no StyleFilterType
   * parameter.
   */
  static ReferenceState GetAndObserveFilters(
      nsISupports* aObserverList, nsTArray<SVGFilterFrame*>* aFilterFrames);

  /**
   * If the given frame is already observing SVG filters, this function gets
   * those filters.  If the frame is not already observing filters this
   * function assumes that it doesn't have anything to observe.
   */
  static ReferenceState GetFiltersIfObserving(
      nsIFrame* aFilteredFrame, nsTArray<SVGFilterFrame*>* aFilterFrames);

  /**
   * Starts observing filters for a <canvas> element's CanvasRenderingContext2D.
   *
   * Returns a RAII object that the caller should make sure is released once
   * the CanvasRenderingContext2D is no longer using them (that is, when the
   * CanvasRenderingContext2D "drawing style state" on which the filters were
   * set is destroyed or has its filter style reset).
   *
   * XXXjwatt: It's a bit unfortunate that both we and
   * CanvasRenderingContext2D::UpdateFilter process the list of StyleFilter
   * objects separately.  It would be better to refactor things so that we only
   * do that work once.
   */
  static already_AddRefed<nsISupports> ObserveFiltersForCanvasContext(
      CanvasRenderingContext2D* aContext, Element* aCanvasElement,
      Span<const StyleFilter> aFilters);

  /**
   * Called when cycle collecting CanvasRenderingContext2D, and requires the
   * RAII object returned from ObserveFiltersForCanvasContext to be passed in.
   *
   * XXXjwatt: I don't think this is doing anything useful.  All we do under
   * this function is clear a raw C-style (i.e. not strong) pointer.  That's
   * clearly not helping in breaking any cycles.  The fact that we MOZ_CRASH
   * in OnRenderingChange if that pointer is null indicates that this isn't
   * even doing anything useful in terms of preventing further invalidation
   * from any observed filters.
   */
  static void DetachFromCanvasContext(nsISupports* aAutoObserver);

  /**
   * Get the frame of the SVG clipPath applied to aClippedFrame, if any, and
   * set up aClippedFrame as a rendering observer of the clipPath's frame, to
   * be invalidated if it changes.
   *
   * Currently we only have support for 'clip-path' with a single item, but the
   * spec. now says 'clip-path' can be set to an arbitrary number of items.
   * Once we support that, aClipPathFrame will need to be an nsTArray as it
   * is for 'filter' and 'mask'.  Currently a return value of eHasNoRefs means
   * that there is no clipping at all, but once we support more than one item
   * then - as for filter and mask - we could still have basic shape clipping
   * to apply even if there are no references to SVG clipPath elements.
   *
   * Note that, unlike for filters, a reference to an ID that doesn't exist
   * is not invalid for clip-path or mask.  We will return eHasNoRefs in that
   * case.
   */
  static ReferenceState GetAndObserveClipPath(
      nsIFrame* aClippedFrame, SVGClipPathFrame** aClipPathFrame);

  /**
   * Get the element of the SVG Shape element, if any, and set up |aFrame| as a
   * rendering observer of the geometry frame, to post a restyle if it changes.
   *
   * We use this function to resolve offset-path:url() and build the equivalent
   * path from this shape element, and generate the transformation from for CSS
   * Motion.
   */
  static SVGGeometryElement* GetAndObserveGeometry(nsIFrame* aFrame);

  /**
   * If masking is applied to aMaskedFrame, gets an array of any SVG masks
   * that are referenced, setting up aMaskFrames as a rendering observer of
   * those masks (if any).
   *
   * NOTE! A return value of eHasNoRefs does NOT mean that there are no masks
   * to be applied, only that there are no references to SVG mask elements.
   *
   * Note that, unlike for filters, a reference to an ID that doesn't exist
   * is not invalid for clip-path or mask.  We will return eHasNoRefs in that
   * case.
   */
  static ReferenceState GetAndObserveMasks(
      nsIFrame* aMaskedFrame, nsTArray<SVGMaskFrame*>* aMaskFrames);

  /**
   * Get the SVGGeometryElement that is referenced by aTextPathFrame, and make
   * aTextPathFrame start observing rendering changes to that element.
   */
  static SVGGeometryElement* GetAndObserveTextPathsPath(
      nsIFrame* aTextPathFrame);

  /**
   * Make aTextPathFrame stop observing rendering changes to the
   * SVGGeometryElement that it references, if any.
   */
  static void RemoveTextPathObserver(nsIFrame* aTextPathFrame);

  /**
   * Get the SVGGeometryElement that is referenced by aSVGMPathElement, and
   * make aSVGMPathElement start observing rendering changes to that element.
   */
  static SVGGeometryElement* GetAndObserveMPathsPath(
      dom::SVGMPathElement* aSVGMPathElement);

  static void TraverseMPathObserver(dom::SVGMPathElement* aSVGMPathElement,
                                    nsCycleCollectionTraversalCallback* aCB);

  /**
   * Gets the nsIFrame of a referenced SVG "template" element, if any, and
   * makes aFrame start observing rendering changes to the template element.
   *
   * Template elements: some elements like gradients, pattern or filter can
   * reference another element of the same type using their 'href' attribute,
   * and use that element as a template that provides attributes or content
   * that is missing from the referring element.
   *
   * The frames that this function is called for do not have a common base
   * class, which is why it is necessary to pass in a function that can be
   * used as a callback to lazily get the href value, if necessary.
   */
  static nsIFrame* GetAndObserveTemplate(nsIFrame* aFrame,
                                         HrefToTemplateCallback aGetHref);

  static void RemoveTemplateObserver(nsIFrame* aFrame);

  /**
   * Gets an arbitrary element and starts observing it.  Used to implement
   * '-moz-element'.
   *
   * Note that bug 1496065 has been filed to remove support for referencing
   * arbitrary elements using '-moz-element'.
   */
  static Element* GetAndObserveBackgroundImage(nsIFrame* aFrame,
                                               const nsAtom* aHref);

  /**
   * Gets an arbitrary element and starts observing it.  Used to detect
   * invalidation changes for background-clip:text.
   */
  static Element* GetAndObserveBackgroundClip(nsIFrame* aFrame);

  /**
   * Return a baseURL for resolving a local-ref URL.
   *
   * @param aContent an element which uses a local-ref property. Here are some
   *                 examples:
   *                   <rect fill=url(#foo)>
   *                   <circle clip-path=url(#foo)>
   *                   <use xlink:href="#foo">
   */
  static already_AddRefed<nsIURI> GetBaseURLForLocalRef(nsIContent* aContent,
                                                        nsIURI* aDocURI);
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGOBSERVERUTILS_H_
