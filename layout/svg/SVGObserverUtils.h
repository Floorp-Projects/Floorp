/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGEFFECTS_H_
#define NSSVGEFFECTS_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDTracker.h"
#include "FrameProperties.h"
#include "mozilla/dom/Element.h"
#include "nsHashKeys.h"
#include "nsID.h"
#include "nsIFrame.h"
#include "nsIMutationObserver.h"
#include "nsInterfaceHashtable.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsStringFwd.h"
#include "nsStubMutationObserver.h"
#include "nsSVGUtils.h"
#include "nsTHashtable.h"
#include "nsURIHashKey.h"
#include "nsCycleCollectionParticipant.h"

class nsAtom;
class nsIPresShell;
class nsIURI;
class nsSVGClipPathFrame;
class nsSVGMarkerFrame;
class nsSVGPaintServerFrame;
class nsSVGFilterFrame;
class nsSVGMaskFrame;
namespace mozilla {
class SVGFilterObserverList;
namespace dom {
class CanvasRenderingContext2D;
class SVGGeometryElement;
}
}

namespace mozilla {

/*
 * This class contains URL and referrer information (referrer and referrer
 * policy).
 * We use it to pass to svg system instead of nsIURI. The object brings referrer
 * and referrer policy so we can send correct Referer headers.
 */
class URLAndReferrerInfo
{
public:
  URLAndReferrerInfo(nsIURI* aURI, nsIURI* aReferrer,
                     mozilla::net::ReferrerPolicy aReferrerPolicy)
    : mURI(aURI)
    , mReferrer(aReferrer)
    , mReferrerPolicy(aReferrerPolicy)
 {
   MOZ_ASSERT(aURI);
 }

  NS_INLINE_DECL_REFCOUNTING(URLAndReferrerInfo)

  nsIURI* GetURI() { return mURI; }
  nsIURI* GetReferrer() { return mReferrer; }
  mozilla::net::ReferrerPolicy GetReferrerPolicy() { return mReferrerPolicy; }

private:
  ~URLAndReferrerInfo() = default;

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mReferrer;
  mozilla::net::ReferrerPolicy mReferrerPolicy;
};

/*
 * This interface allows us to be notified when a piece of SVG content is
 * re-rendered.
 *
 * Concrete implementations of this interface need to implement
 * "GetTarget()" to specify the piece of SVG content that they'd like to
 * monitor, and they need to implement "OnRenderingChange" to specify how
 * we'll react when that content gets re-rendered. They also need to implement
 * a constructor and destructor, which should call StartObserving and
 * StopObserving, respectively.
 */
class SVGRenderingObserver : public nsStubMutationObserver
{

protected:
  virtual ~SVGRenderingObserver() = default;

public:
  typedef mozilla::dom::Element Element;

  SVGRenderingObserver()
    : mInObserverList(false)
  {}

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
  void NotifyEvictedFromRenderingObserverList();

  nsIFrame* GetReferencedFrame();
  /**
   * @param aOK this is only for the convenience of callers. We set *aOK to false
   * if the frame is the wrong type
   */
  nsIFrame* GetReferencedFrame(mozilla::LayoutFrameType aFrameType, bool* aOK);

  Element* GetReferencedElement();

  virtual bool ObservesReflow() { return true; }

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

  // This is an internally-used version of GetReferencedElement that doesn't
  // forcibly add us as an observer. (whereas GetReferencedElement does)
  virtual Element* GetTarget() = 0;

  // Whether we're in our referenced element's observer list at this time.
  bool mInObserverList;
};


/*
 * SVG elements reference supporting resources by element ID. We need to
 * track when those resources change and when the DOM changes in ways
 * that affect which element is referenced by a given ID (e.g., when
 * element IDs change). The code here is responsible for that.
 *
 * When a frame references a supporting resource, we create a property
 * object derived from SVGIDRenderingObserver to manage the relationship. The
 * property object is attached to the referencing frame.
 */
class SVGIDRenderingObserver : public SVGRenderingObserver
{
public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::dom::IDTracker IDTracker;

  SVGIDRenderingObserver(URLAndReferrerInfo* aURI, nsIContent* aObservingContent,
                         bool aReferenceImage);
  virtual ~SVGIDRenderingObserver();

protected:
  Element* GetTarget() override { return mObservedElementTracker.get(); }

  void OnRenderingChange() override;

  /**
   * Helper that provides a reference to the element with the ID that our
   * observer wants to observe, and that will invalidate our observer if the
   * element that that ID identifies changes to a different element (or none).
   */
  class ElementTracker final : public IDTracker
  {
  public:
    explicit ElementTracker(SVGIDRenderingObserver* aOwningObserver)
      : mOwningObserver(aOwningObserver)
    {}
  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) override {
      mOwningObserver->StopObserving(); // stop observing the old element
      IDTracker::ElementChanged(aFrom, aTo);
      mOwningObserver->StartObserving(); // start observing the new element
      mOwningObserver->OnRenderingChange();
    }
    /**
     * Override IsPersistent because we want to keep tracking the element
     * for the ID even when it changes.
     */
    virtual bool IsPersistent() override { return true; }
  private:
    SVGIDRenderingObserver* mOwningObserver;
  };

  ElementTracker mObservedElementTracker;
};

struct nsSVGFrameReferenceFromProperty
{
  explicit nsSVGFrameReferenceFromProperty(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mFramePresShell(aFrame->PresShell())
  {}

  // Clear our reference to the frame.
  void Detach();

  // null if the frame has become invalid
  nsIFrame* Get();

private:
  // The frame that this property is attached to, may be null
  nsIFrame *mFrame;
  // When a presshell is torn down, we don't delete the properties for
  // each frame until after the frames are destroyed. So here we remember
  // the presshell for the frames we care about and, before we use the frame,
  // we test the presshell to see if it's destroying itself. If it is,
  // then the frame pointer is not valid and we know the frame has gone away.
  // mFramePresShell may be null, but when mFrame is non-null, mFramePresShell
  // is guaranteed to be non-null, too.
  nsIPresShell *mFramePresShell;
};

/**
 * Used for gradient-to-gradient, pattern-to-pattern and filter-to-filter
 * references to "template" elements (specified via the 'href' attributes).
 *
 * This is a special class for the case where we know we only want to call
 * InvalidateDirectRenderingObservers (as opposed to
 * InvalidateRenderingObservers).
 *
 * TODO(jwatt): If we added a new NS_FRAME_RENDERING_OBSERVER_CONTAINER state
 * bit to clipPath, filter, gradients, marker, mask, pattern and symbol, and
 * could have InvalidateRenderingObservers stop on reaching such an element,
 * then we would no longer need this class (not to mention improving perf by
 * significantly cutting down on ancestor traversal).
 */
class SVGTemplateElementObserver : public SVGIDRenderingObserver
{
public:
  NS_DECL_ISUPPORTS

  SVGTemplateElementObserver(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
                             bool aReferenceImage)
    : SVGIDRenderingObserver(aURI, aFrame->GetContent(), aReferenceImage)
    , mFrameReference(aFrame)
  {}

protected:
  virtual ~SVGTemplateElementObserver() = default; // non-public

  virtual void OnRenderingChange() override;

  nsSVGFrameReferenceFromProperty mFrameReference;
};

class nsSVGRenderingObserverProperty : public SVGIDRenderingObserver
{
public:
  NS_DECL_ISUPPORTS

  nsSVGRenderingObserverProperty(URLAndReferrerInfo* aURI, nsIFrame *aFrame,
                                 bool aReferenceImage)
    : SVGIDRenderingObserver(aURI, aFrame->GetContent(), aReferenceImage)
    , mFrameReference(aFrame)
  {}

protected:
  virtual ~nsSVGRenderingObserverProperty() {}

  virtual void OnRenderingChange() override;

  nsSVGFrameReferenceFromProperty mFrameReference;
};

/**
 * In a filter chain, there can be multiple SVG reference filters.
 * e.g. filter: url(#svg-filter-1) blur(10px) url(#svg-filter-2);
 *
 * This class keeps track of one SVG reference filter in a filter chain.
 * e.g. url(#svg-filter-1)
 *
 * It fires invalidations when the SVG filter element's id changes or when
 * the SVG filter element's content changes.
 *
 * The SVGFilterObserverList class manages a list of SVGFilterObservers.
 */
class SVGFilterObserver final : public SVGIDRenderingObserver
{
public:
  SVGFilterObserver(URLAndReferrerInfo* aURI,
                    nsIContent* aObservingContent,
                    SVGFilterObserverList* aFilterChainObserver)
    : SVGIDRenderingObserver(aURI, aObservingContent, false)
    , mFilterObserverList(aFilterChainObserver)
  {
  }

  bool ReferencesValidResource() { return GetFilterFrame(); }

  void DetachFromChainObserver() { mFilterObserverList = nullptr; }

  /**
   * @return the filter frame, or null if there is no filter frame
   */
  nsSVGFilterFrame *GetFilterFrame();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SVGFilterObserver)

  void Invalidate() { OnRenderingChange(); };

protected:
  virtual ~SVGFilterObserver() {}

  // SVGIDRenderingObserver
  virtual void OnRenderingChange() override;

private:
  SVGFilterObserverList* mFilterObserverList;
};

/**
 * This class manages a list of SVGFilterObservers, which correspond to
 * reference to SVG filters in a list of filters in a given 'filter' property.
 * e.g. filter: url(#svg-filter-1) blur(10px) url(#svg-filter-2);
 *
 * In the above example, the SVGFilterObserverList will manage two
 * SVGFilterObservers, one for each of the references to SVG filters.  CSS
 * filters like "blur(10px)" don't reference filter elements, so they don't
 * need an SVGFilterObserver.  The style system invalidates changes to CSS
 * filters.
 */
class SVGFilterObserverList : public nsISupports
{
public:
  SVGFilterObserverList(const nsTArray<nsStyleFilter>& aFilters,
                        nsIContent* aFilteredElement,
                        nsIFrame* aFiltedFrame = nullptr);

  bool ReferencesValidResources();
  void Invalidate() { OnRenderingChange(); }

  const nsTArray<RefPtr<SVGFilterObserver>>& GetObservers() const {
    return mObservers;
  }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SVGFilterObserverList)

protected:
  virtual ~SVGFilterObserverList();

  virtual void OnRenderingChange() = 0;

private:

  void DetachObservers()
  {
    for (uint32_t i = 0; i < mObservers.Length(); i++) {
      mObservers[i]->DetachFromChainObserver();
    }
  }

  nsTArray<RefPtr<SVGFilterObserver>> mObservers;
};

class SVGFilterObserverListForCSSProp final : public SVGFilterObserverList
{
public:
  SVGFilterObserverListForCSSProp(const nsTArray<nsStyleFilter>& aFilters,
                                  nsIFrame* aFilteredFrame)
    : SVGFilterObserverList(aFilters, aFilteredFrame->GetContent(),
                            aFilteredFrame)
    , mFrameReference(aFilteredFrame)
  {}

  void DetachFromFrame() { mFrameReference.Detach(); }

protected:
  virtual void OnRenderingChange() override;

  nsSVGFrameReferenceFromProperty mFrameReference;
};

class SVGMarkerObserver final: public nsSVGRenderingObserverProperty
{
public:
  SVGMarkerObserver(URLAndReferrerInfo* aURI, nsIFrame* aFrame, bool aReferenceImage)
    : nsSVGRenderingObserverProperty(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void OnRenderingChange() override;
};

class SVGTextPathObserver final : public nsSVGRenderingObserverProperty
{
public:
  SVGTextPathObserver(URLAndReferrerInfo* aURI, nsIFrame* aFrame, bool aReferenceImage)
    : nsSVGRenderingObserverProperty(aURI, aFrame, aReferenceImage)
    , mValid(true) {}

  virtual bool ObservesReflow() override { return false; }

protected:
  virtual void OnRenderingChange() override;

private:
  /**
   * Returns true if the target of the textPath is the frame of a 'path' element.
   */
  bool TargetIsValid();

  bool mValid;
};

class nsSVGPaintingProperty final : public nsSVGRenderingObserverProperty
{
public:
  nsSVGPaintingProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame, bool aReferenceImage)
    : nsSVGRenderingObserverProperty(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void OnRenderingChange() override;
};

class SVGMaskObserverList final : public nsISupports
{
public:
  explicit SVGMaskObserverList(nsIFrame* aFrame);

  // nsISupports
  NS_DECL_ISUPPORTS

  const nsTArray<RefPtr<nsSVGPaintingProperty>>& GetObservers() const
  {
    return mProperties;
  }

  void ResolveImage(uint32_t aIndex);

private:
  virtual ~SVGMaskObserverList() {}
  nsTArray<RefPtr<nsSVGPaintingProperty>> mProperties;
  nsIFrame* mFrame;
};

/**
 * A manager for one-shot SVGRenderingObserver tracking.
 * nsSVGRenderingObservers can be added or removed. They are not strongly
 * referenced so an observer must be removed before it dies.
 * When InvalidateAll is called, all outstanding references get
 * OnNonDOMMutationRenderingChange()
 * called on them and the list is cleared. The intent is that
 * the observer will force repainting of whatever part of the document
 * is needed, and then at paint time the observer will do a clean lookup
 * of the referenced element and [re-]add itself to the element's observer list.
 *
 * InvalidateAll must be called before this object is destroyed, i.e.
 * before the referenced frame is destroyed. This should normally happen
 * via nsSVGContainerFrame::RemoveFrame, since only frames in the frame
 * tree should be referenced.
 */
class SVGRenderingObserverList
{
public:
  SVGRenderingObserverList()
    : mObservers(4)
  {
    MOZ_COUNT_CTOR(SVGRenderingObserverList);
  }

  ~SVGRenderingObserverList() {
    InvalidateAll();
    MOZ_COUNT_DTOR(SVGRenderingObserverList);
  }

  void Add(SVGRenderingObserver* aObserver)
  { mObservers.PutEntry(aObserver); }
  void Remove(SVGRenderingObserver* aObserver)
  { mObservers.RemoveEntry(aObserver); }
#ifdef DEBUG
  bool Contains(SVGRenderingObserver* aObserver)
  { return (mObservers.GetEntry(aObserver) != nullptr); }
#endif
  bool IsEmpty()
  { return mObservers.Count() == 0; }

  /**
   * Drop all our observers, and notify them that we have changed and dropped
   * our reference to them.
   */
  void InvalidateAll();

  /**
   * Drop all observers that observe reflow, and notify them that we have changed and dropped
   * our reference to them.
   */
  void InvalidateAllForReflow();

  /**
   * Drop all our observers, and notify them that we have dropped our reference
   * to them.
   */
  void RemoveAll();

private:
  nsTHashtable<nsPtrHashKey<SVGRenderingObserver>> mObservers;
};

class SVGObserverUtils
{
public:
  typedef mozilla::dom::CanvasRenderingContext2D CanvasRenderingContext2D;
  typedef mozilla::dom::Element Element;
  typedef dom::SVGGeometryElement SVGGeometryElement;
  typedef nsInterfaceHashtable<nsRefPtrHashKey<URLAndReferrerInfo>,
    nsIMutationObserver> URIObserverHashtable;

  using PaintingPropertyDescriptor =
    const mozilla::FramePropertyDescriptor<nsSVGPaintingProperty>*;
  using URIObserverHashtablePropertyDescriptor =
    const mozilla::FramePropertyDescriptor<URIObserverHashtable>*;

  static void DestroyFilterProperty(SVGFilterObserverListForCSSProp* aProp)
  {
    // SVGFilterObserverListForCSSProp is cycle-collected, so dropping the last
    // reference doesn't necessarily destroy it. We need to tell it that the
    // frame has now become invalid.
    aProp->DetachFromFrame();

    aProp->Release();
  }

  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(HrefToTemplateProperty,
                                       SVGTemplateElementObserver)
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(FilterProperty,
                                      SVGFilterObserverListForCSSProp,
                                      DestroyFilterProperty)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MaskProperty, SVGMaskObserverList)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(ClipPathProperty, nsSVGPaintingProperty)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MarkerStartProperty, SVGMarkerObserver)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MarkerMidProperty, SVGMarkerObserver)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MarkerEndProperty, SVGMarkerObserver)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(FillProperty, nsSVGPaintingProperty)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(StrokeProperty, nsSVGPaintingProperty)
  NS_DECLARE_FRAME_PROPERTY_RELEASABLE(HrefAsTextPathProperty,
                                       SVGTextPathObserver)
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(BackgroundImageProperty,
                                      URIObserverHashtable)

  struct EffectProperties {
    SVGMaskObserverList* mMaskObservers;
    nsSVGPaintingProperty* mClipPath;

    /**
     * @return the clip-path frame, or null if there is no clip-path frame
     */
    nsSVGClipPathFrame* GetClipPathFrame();

    /**
     * @return an array which contains all SVG mask frames.
     */
    nsTArray<nsSVGMaskFrame*> GetMaskFrames();

    /*
     * @return true if all effects we have are valid or we have no effect
     * at all.
     */
    bool HasNoOrValidEffects();

    /*
     * @return true if we have any invalid effect.
     */
    bool HasInvalidEffects() {
      return !HasNoOrValidEffects();
    }

    /*
     * @return true if we either do not have clip-path or have a valid
     * clip-path.
     */
    bool HasNoOrValidClipPath();

    /*
     * @return true if we have an invalid clip-path.
     */
    bool HasInvalidClipPath() {
      return !HasNoOrValidClipPath();
    }

    /*
     * @return true if we either do not have mask or all masks we have
     * are valid.
     */
    bool HasNoOrValidMask();

    /*
     * @return true if we have an invalid mask.
     */
    bool HasInvalidMask() {
      return !HasNoOrValidMask();
    }
  };

  /**
   * @param aFrame should be the first continuation
   */
  static EffectProperties GetEffectProperties(nsIFrame* aFrame);

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
   * @param aFrame should be the first continuation
   */
  static SVGFilterObserverListForCSSProp* GetFilterObserverList(nsIFrame* aFrame);

  /**
   * @param aFrame must be a first-continuation.
   */
  static void AddRenderingObserver(Element* aElement,
                                   SVGRenderingObserver *aObserver);
  /**
   * @param aFrame must be a first-continuation.
   */
  static void RemoveRenderingObserver(Element* aElement,
                                      SVGRenderingObserver *aObserver);

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
   * nsNodeUtils::AttributeChanged which walks up the content node tree all the
   * way to the root node (not stopping if it encounters a non-container SVG
   * node) invalidating all mutation observers (not just
   * nsSVGRenderingObservers) on all nodes along the way (not just the first
   * node it finds with observers). In other words, by doing all the
   * things in parentheses in the preceding sentence, this method uses
   * knowledge about our implementation and what can be affected by SVG effects
   * to make invalidation relatively lightweight when an SVG effect changes.
   */
  static void InvalidateRenderingObservers(nsIFrame* aFrame);

  enum {
    INVALIDATE_REFLOW = 1
  };

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
  static void InvalidateDirectRenderingObservers(Element* aElement, uint32_t aFlags = 0);
  static void InvalidateDirectRenderingObservers(nsIFrame* aFrame, uint32_t aFlags = 0);

  /**
   * Get the paint server for a aTargetFrame.
   */
  static nsSVGPaintServerFrame *GetPaintServer(nsIFrame* aTargetFrame,
                                               nsStyleSVGPaint nsStyleSVG::* aPaint);

  /**
   * Get the start/mid/end-markers for the given frame, and add the frame as
   * an observer to those markers.  Returns true if at least one marker type is
   * found, false otherwise.
   */
  static bool
  GetMarkerFrames(nsIFrame* aMarkedFrame, nsSVGMarkerFrame*(*aFrames)[3]);

  /**
   * Get the frames of the SVG filters applied to the given frame, and add the
   * frame as an observer to those filter frames.
   *
   * NOTE! A return value of eHasNoRefs does NOT mean that there are no filters
   * to be applied, only that there are no references to SVG filter elements.
   *
   * XXX Callers other than ComputePostEffectsVisualOverflowRect and
   * nsSVGUtils::GetPostFilterVisualOverflowRect should not need to initiate
   * observing.  If we have a bug that causes invalidation (which would remove
   * observers) between reflow and painting, then we don't really want to
   * re-add abservers during painting.  That has the potential to hide logic
   * bugs, or cause later invalidation problems.  However, let's not change
   * that behavior just yet due to the regression potential.
   */
  static ReferenceState
  GetAndObserveFilters(nsIFrame* aFilteredFrame,
                       nsTArray<nsSVGFilterFrame*>* aFilterFrames);

  /**
   * If the given frame is already observing SVG filters, this function gets
   * those filters.  If the frame is not already observing filters this
   * function assumes that it doesn't have anything to observe.
   */
  static ReferenceState
  GetFiltersIfObserving(nsIFrame* aFilteredFrame,
                        nsTArray<nsSVGFilterFrame*>* aFilterFrames);

  /**
   * Starts observing filters for a <canvas> element's CanvasRenderingContext2D.
   *
   * Returns a RAII object that the caller should make sure is released once
   * the CanvasRenderingContext2D is no longer using them (that is, when the
   * CanvasRenderingContext2D "drawing style state" on which the filters were
   * set is destroyed or has its filter style reset).
   *
   * XXXjwatt: It's a bit unfortunate that both we and
   * CanvasRenderingContext2D::UpdateFilter process the list of nsStyleFilter
   * objects separately.  It would be better to refactor things so that we only
   * do that work once.
   */
  static already_AddRefed<nsISupports>
  ObserveFiltersForCanvasContext(CanvasRenderingContext2D* aContext,
                                 Element* aCanvasElement,
                                 nsTArray<nsStyleFilter>& aFilters);

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
  static void
  DetachFromCanvasContext(nsISupports* aAutoObserver);

  /**
   * Get the SVGGeometryElement that is referenced by aTextPathFrame, and make
   * aTextPathFrame start observing rendering changes to that element.
   */
  static SVGGeometryElement*
  GetTextPathsReferencedPath(nsIFrame* aTextPathFrame);

  /**
   * Make aTextPathFrame stop observing rendering changes to the
   * SVGGeometryElement that it references, if any.
   */
  static void
  RemoveTextPathObserver(nsIFrame* aTextPathFrame);

  using HrefToTemplateCallback = const std::function<void(nsAString&)>&;
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
  static nsIFrame*
  GetTemplateFrame(nsIFrame* aFrame, HrefToTemplateCallback aGetHref);

  static void
  RemoveTemplateObserver(nsIFrame* aFrame);

  /**
   * Get an nsSVGPaintingProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGPaintingProperty*
  GetPaintingProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
      const mozilla::FramePropertyDescriptor<nsSVGPaintingProperty>* aProperty);
  /**
   * Get an nsSVGPaintingProperty for the frame for that URI, creating a fresh
   * one if necessary
   */
  static nsSVGPaintingProperty*
  GetPaintingPropertyForURI(URLAndReferrerInfo* aURI,
                            nsIFrame* aFrame,
                            URIObserverHashtablePropertyDescriptor aProp);

  /**
   * A helper function to resolve marker's URL.
   */
  static already_AddRefed<URLAndReferrerInfo>
  GetMarkerURI(nsIFrame* aFrame,
               RefPtr<mozilla::css::URLValue> nsStyleSVG::* aMarker);

  /**
   * A helper function to resolve clip-path URL.
   */
  static already_AddRefed<URLAndReferrerInfo>
  GetClipPathURI(nsIFrame* aFrame);

  /**
   * A helper function to resolve filter URL.
   */
  static already_AddRefed<URLAndReferrerInfo>
  GetFilterURI(nsIFrame* aFrame, uint32_t aIndex);

  /**
   * A helper function to resolve filter URL.
   */
  static already_AddRefed<URLAndReferrerInfo>
  GetFilterURI(nsIFrame* aFrame, const nsStyleFilter& aFilter);

  /**
   * A helper function to resolve paint-server URL.
   */
  static already_AddRefed<URLAndReferrerInfo>
  GetPaintURI(nsIFrame* aFrame, nsStyleSVGPaint nsStyleSVG::* aPaint);

  /**
   * A helper function to resolve SVG mask URL.
   */
  static already_AddRefed<URLAndReferrerInfo>
  GetMaskURI(nsIFrame* aFrame, uint32_t aIndex);

  /**
   * Return a baseURL for resolving a local-ref URL.
   *
   * @param aContent an element which uses a local-ref property. Here are some
   *                 examples:
   *                   <rect fill=url(#foo)>
   *                   <circle clip-path=url(#foo)>
   *                   <use xlink:href="#foo">
   */
  static already_AddRefed<nsIURI>
  GetBaseURLForLocalRef(nsIContent* aContent, nsIURI* aDocURI);
};

} // namespace mozilla

#endif /*NSSVGEFFECTS_H_*/
