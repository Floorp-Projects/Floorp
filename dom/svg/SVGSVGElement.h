/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGSVGElement_h
#define mozilla_dom_SVGSVGElement_h

#include "SVGViewportElement.h"

nsresult NS_NewSVGSVGElement(nsIContent **aResult,
                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                             mozilla::dom::FromParser aFromParser);

class nsSMILTimeContainer;

namespace mozilla {
class AutoSVGViewHandler;
class SVGFragmentIdentifier;
class EventChainPreVisitor;
class DOMSVGLength;
class DOMSVGNumber;

namespace dom {
class SVGAngle;
class SVGMatrix;
class SVGIRect;
class SVGSVGElement;

// Stores svgView arguments of SVG fragment identifiers.
class SVGView {
public:
  SVGView();

  nsSVGEnum                             mZoomAndPan;
  nsSVGViewBox                          mViewBox;
  SVGAnimatedPreserveAspectRatio        mPreserveAspectRatio;
  nsAutoPtr<nsSVGAnimatedTransformList> mTransforms;
};

class DOMSVGTranslatePoint final : public nsISVGPoint {
public:
  DOMSVGTranslatePoint(SVGPoint* aPt, SVGSVGElement* aElement)
    : nsISVGPoint(aPt, true), mElement(aElement) {}

  explicit DOMSVGTranslatePoint(DOMSVGTranslatePoint* aPt)
    : nsISVGPoint(&aPt->mPt, true), mElement(aPt->mElement) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMSVGTranslatePoint, nsISVGPoint)

  virtual DOMSVGPoint* Copy() override;

  // WebIDL
  virtual float X() override { return mPt.GetX(); }
  virtual float Y() override { return mPt.GetY(); }
  virtual void SetX(float aValue, ErrorResult& rv) override;
  virtual void SetY(float aValue, ErrorResult& rv) override;
  virtual already_AddRefed<nsISVGPoint> MatrixTransform(SVGMatrix& matrix) override;

  virtual nsISupports* GetParentObject() override;

  RefPtr<SVGSVGElement> mElement;

private:
  ~DOMSVGTranslatePoint() {}
};

typedef SVGViewportElement SVGSVGElementBase;

class SVGSVGElement final : public SVGSVGElementBase
{
  friend class ::nsSVGOuterSVGFrame;
  friend class mozilla::SVGFragmentIdentifier;
  friend class mozilla::AutoSVGViewHandler;
  friend class mozilla::AutoPreserveAspectRatioOverride;
  friend class mozilla::dom::SVGView;

protected:
  SVGSVGElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                FromParser aFromParser);
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  friend nsresult (::NS_NewSVGSVGElement(nsIContent **aResult,
                                         already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                                         mozilla::dom::FromParser aFromParser));

  ~SVGSVGElement();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGSVGElement, SVGSVGElementBase)

  /**
   * For use by zoom controls to allow currentScale, currentTranslate.x and
   * currentTranslate.y to be set by a single operation that dispatches a
   * single SVGZoom event (instead of one SVGZoom and two SVGScroll events).
   *
   * XXX SVGZoomEvent is no more, is this needed?
   */
  void SetCurrentScaleTranslate(float s, float x, float y);

  // nsIContent interface
  virtual nsresult GetEventTargetParent(
                     EventChainPreVisitor& aVisitor) override;
  virtual bool IsEventAttributeNameInternal(nsIAtom* aName) override;

  // nsINode methods:
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();
  float PixelUnitToMillimeterX();
  float PixelUnitToMillimeterY();
  float ScreenPixelToMillimeterX();
  float ScreenPixelToMillimeterY();
  bool UseCurrentView();
  float CurrentScale();
  void SetCurrentScale(float aCurrentScale);
  already_AddRefed<nsISVGPoint> CurrentTranslate();
  void SetCurrentTranslate(float x, float y);
  uint32_t SuspendRedraw(uint32_t max_wait_milliseconds);
  void UnsuspendRedraw(uint32_t suspend_handle_id);
  void UnsuspendRedrawAll();
  void ForceRedraw();
  void PauseAnimations();
  void UnpauseAnimations();
  bool AnimationsPaused();
  float GetCurrentTime();
  void SetCurrentTime(float seconds);
  void DeselectAll();
  already_AddRefed<DOMSVGNumber> CreateSVGNumber();
  already_AddRefed<DOMSVGLength> CreateSVGLength();
  already_AddRefed<SVGAngle> CreateSVGAngle();
  already_AddRefed<nsISVGPoint> CreateSVGPoint();
  already_AddRefed<SVGMatrix> CreateSVGMatrix();
  already_AddRefed<SVGIRect> CreateSVGRect();
  already_AddRefed<SVGTransform> CreateSVGTransform();
  already_AddRefed<SVGTransform> CreateSVGTransformFromMatrix(SVGMatrix& matrix);
  using nsINode::GetElementById; // This does what we want
  uint16_t ZoomAndPan();
  void SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv);

  // nsSVGElement overrides

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;
  virtual nsSVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0) override;

  // SVGSVGElement methods:

  // Returns true IFF our attributes are currently overridden by a <view>
  // element and that element's ID matches the passed-in string.
  bool IsOverriddenBy(const nsAString &aViewID) const {
    return mCurrentViewID && mCurrentViewID->Equals(aViewID);
  }

  nsSMILTimeContainer* GetTimedDocumentRoot();

  // public helpers:

  /**
   * Returns -1 if the width/height is a percentage, else returns the user unit
   * length clamped to fit in a int32_t.
   * XXX see bug 1112533 comment 3 - we should fix drawImage so that we can
   * change these methods to make zero the error flag for percentages.
   */
  int32_t GetIntrinsicWidth();
  int32_t GetIntrinsicHeight();

  // This services any pending notifications for the transform on on this root
  // <svg> node needing to be recalculated.  (Only applicable in
  // SVG-as-an-image documents.)
  virtual void FlushImageTransformInvalidation();

  svgFloatSize GetViewportSize() const {
    return svgFloatSize(mViewportWidth, mViewportHeight);
  }

  void SetViewportSize(const svgFloatSize& aSize) {
    mViewportWidth  = aSize.width;
    mViewportHeight = aSize.height;
  }

private:
  // SVGViewportElement methods:

  virtual SVGViewElement* GetCurrentViewElement() const;
  virtual SVGPreserveAspectRatio GetPreserveAspectRatioWithOverride() const override;

  // implementation helpers:

  /*
   * While binding to the tree we need to determine if we will be the outermost
   * <svg> element _before_ the children are bound (as they want to know what
   * timed document root to register with) and therefore _before_ our parent is
   * set (both actions are performed by Element::BindToTree) so we
   * can't use GetOwnerSVGElement() as it relies on GetParent(). This code is
   * basically a simplified version of GetOwnerSVGElement that uses the parent
   * parameters passed in instead.
   */
  bool WillBeOutermostSVG(nsIContent* aParent,
                          nsIContent* aBindingParent) const;

  // invalidate viewbox -> viewport xform & inform frames
  void InvalidateTransformNotifyFrame();

  // Methods for <image> elements to override my "PreserveAspectRatio" value.
  // These are private so that only our friends
  // (AutoPreserveAspectRatioOverride in particular) have access.
  void SetImageOverridePreserveAspectRatio(const SVGPreserveAspectRatio& aPAR);
  void ClearImageOverridePreserveAspectRatio();

  // Set/Clear properties to hold old version of preserveAspectRatio
  // when it's being overridden by an <image> element that we are inside of.
  bool SetPreserveAspectRatioProperty(const SVGPreserveAspectRatio& aPAR);
  const SVGPreserveAspectRatio* GetPreserveAspectRatioProperty() const;
  bool ClearPreserveAspectRatioProperty();

  virtual SVGPoint GetCurrentTranslate() const override
  { return mCurrentTranslate; }
  virtual float GetCurrentScale() const override
  { return mCurrentScale; }

  virtual const nsSVGViewBox& GetViewBoxInternal() const override;
  virtual nsSVGAnimatedTransformList* GetTransformInternal() const override;

  virtual EnumAttributesInfo GetEnumInfo() override;

  enum { ZOOMANDPAN };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  // The time container for animations within this SVG document fragment. Set
  // for all outermost <svg> elements (not nested <svg> elements).
  nsAutoPtr<nsSMILTimeContainer> mTimedDocumentRoot;

  // zoom and pan
  // IMPORTANT: see the comment in RecordCurrentScaleTranslate before writing
  // code to change any of these!
  SVGPoint mCurrentTranslate;
  float    mCurrentScale;
  SVGPoint mPreviousTranslate;
  float    mPreviousScale;

  // For outermost <svg> elements created from parsing, animation is started by
  // the onload event in accordance with the SVG spec, but for <svg> elements
  // created by script or promoted from inner <svg> to outermost <svg> we need
  // to manually kick off animation when they are bound to the tree.
  bool     mStartAnimationOnBindToTree;

  bool     mImageNeedsTransformInvalidation;

  // mCurrentViewID and mSVGView are mutually exclusive; we can have
  // at most one non-null.
  nsAutoPtr<nsString>            mCurrentViewID;
  nsAutoPtr<SVGView>             mSVGView;
};

} // namespace dom

class MOZ_RAII AutoSVGTimeSetRestore
{
public:
  AutoSVGTimeSetRestore(dom::SVGSVGElement* aRootElem,
                        float aFrameTime
                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mRootElem(aRootElem)
    , mOriginalTime(mRootElem->GetCurrentTime())
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mRootElem->SetCurrentTime(aFrameTime); // Does nothing if there's no change.
  }

  ~AutoSVGTimeSetRestore()
  {
    mRootElem->SetCurrentTime(mOriginalTime);
  }

private:
  const RefPtr<dom::SVGSVGElement> mRootElem;
  const float mOriginalTime;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoPreserveAspectRatioOverride
{
public:
  AutoPreserveAspectRatioOverride(const Maybe<SVGImageContext>& aSVGContext,
                                  dom::SVGSVGElement* aRootElem
                                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mRootElem(aRootElem)
    , mDidOverride(false)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mRootElem, "No SVG/Symbol node to manage?");

    if (aSVGContext.isSome() &&
        aSVGContext->GetPreserveAspectRatio().isSome()) {
      // Override preserveAspectRatio in our helper document.
      // XXXdholbert We should technically be overriding the helper doc's clip
      // and overflow properties here, too. See bug 272288 comment 36.
      mRootElem->SetImageOverridePreserveAspectRatio(
                   *aSVGContext->GetPreserveAspectRatio());
      mDidOverride = true;
    }
  }

  ~AutoPreserveAspectRatioOverride()
  {
    if (mDidOverride) {
      mRootElem->ClearImageOverridePreserveAspectRatio();
    }
  }

private:
  const RefPtr<dom::SVGSVGElement> mRootElem;
  bool mDidOverride;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} // namespace mozilla

#endif // SVGSVGElement_h
