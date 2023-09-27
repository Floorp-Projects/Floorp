/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGSVGELEMENT_H_
#define DOM_SVG_SVGSVGELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGViewportElement.h"

nsresult NS_NewSVGSVGElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser);

// {4b83982c-e5e9-4ca1-abd4-14d27e8b3531}
#define MOZILLA_SVGSVGELEMENT_IID                    \
  {                                                  \
    0x4b83982c, 0xe5e9, 0x4ca1, {                    \
      0xab, 0xd4, 0x14, 0xd2, 0x7e, 0x8b, 0x35, 0x31 \
    }                                                \
  }

namespace mozilla {
class AutoSVGViewHandler;
class SMILTimeContainer;
class SVGFragmentIdentifier;
class EventChainPreVisitor;

namespace dom {
struct DOMMatrix2DInit;
class DOMSVGAngle;
class DOMSVGLength;
class DOMSVGNumber;
class DOMSVGPoint;
class SVGMatrix;
class SVGRect;
class SVGSVGElement;

// Stores svgView arguments of SVG fragment identifiers.
class SVGView {
 public:
  SVGView();

  SVGAnimatedEnumeration mZoomAndPan;
  SVGAnimatedViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
  UniquePtr<SVGAnimatedTransformList> mTransforms;
};

using SVGSVGElementBase = SVGViewportElement;

class SVGSVGElement final : public SVGSVGElementBase {
  friend class mozilla::SVGFragmentIdentifier;
  friend class mozilla::SVGOuterSVGFrame;
  friend class mozilla::AutoSVGViewHandler;
  friend class mozilla::AutoPreserveAspectRatioOverride;
  friend class mozilla::dom::SVGView;

 protected:
  SVGSVGElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                FromParser aFromParser);
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

  friend nsresult(::NS_NewSVGSVGElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      mozilla::dom::FromParser aFromParser));

  ~SVGSVGElement() = default;

 public:
  NS_IMPL_FROMNODE_WITH_TAG(SVGSVGElement, kNameSpaceID_SVG, svg)

  // interfaces:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_SVGSVGELEMENT_IID)
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGSVGElement, SVGSVGElementBase)

  /*
   * Send appropriate events and updates if our root translate
   * has changed.
   */
  MOZ_CAN_RUN_SCRIPT
  void DidChangeTranslate();

  // nsIContent interface
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  bool IsEventAttributeNameInternal(nsAtom* aName) override;

  // nsINode methods:
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();
  bool UseCurrentView() const;
  float CurrentScale() const;
  void SetCurrentScale(float aCurrentScale);
  already_AddRefed<DOMSVGPoint> CurrentTranslate();
  uint32_t SuspendRedraw(uint32_t max_wait_milliseconds);
  void UnsuspendRedraw(uint32_t suspend_handle_id);
  void UnsuspendRedrawAll();
  void ForceRedraw();
  void PauseAnimations();
  void UnpauseAnimations();
  bool AnimationsPaused();
  float GetCurrentTimeAsFloat();
  void SetCurrentTime(float seconds);
  void DeselectAll();
  already_AddRefed<DOMSVGNumber> CreateSVGNumber();
  already_AddRefed<DOMSVGLength> CreateSVGLength();
  already_AddRefed<DOMSVGAngle> CreateSVGAngle();
  already_AddRefed<DOMSVGPoint> CreateSVGPoint();
  already_AddRefed<SVGMatrix> CreateSVGMatrix();
  already_AddRefed<SVGRect> CreateSVGRect();
  already_AddRefed<DOMSVGTransform> CreateSVGTransform();
  already_AddRefed<DOMSVGTransform> CreateSVGTransformFromMatrix(
      const DOMMatrix2DInit& matrix, ErrorResult& rv);
  using nsINode::GetElementById;  // This does what we want
  uint16_t ZoomAndPan() const;
  void SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv);

  // SVGElement overrides

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent) override;
  SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) override;

  // SVGSVGElement methods:

  // Returns true IFF our attributes are currently overridden by a <view>
  // element and that element's ID matches the passed-in string.
  bool IsOverriddenBy(const nsAString& aViewID) const {
    return mCurrentViewID && mCurrentViewID->Equals(aViewID);
  }

  SMILTimeContainer* GetTimedDocumentRoot();

  // public helpers:

  const SVGPoint& GetCurrentTranslate() const { return mCurrentTranslate; }
  bool IsScaledOrTranslated() const {
    return mCurrentTranslate != SVGPoint() || mCurrentScale != 1.0f;
  }

  LengthPercentage GetIntrinsicWidth();
  LengthPercentage GetIntrinsicHeight();

  // This services any pending notifications for the transform on on this root
  // <svg> node needing to be recalculated.  (Only applicable in
  // SVG-as-an-image documents.)
  virtual void FlushImageTransformInvalidation();

 private:
  // SVGViewportElement methods:

  virtual SVGViewElement* GetCurrentViewElement() const;
  SVGPreserveAspectRatio GetPreserveAspectRatioWithOverride() const override;

  // implementation helpers:

  /*
   * While binding to the tree we need to determine if we will be the outermost
   * <svg> element _before_ the children are bound (as they want to know what
   * timed document root to register with) and therefore _before_ our parent is
   * set (both actions are performed by Element::BindToTree) so we
   * can't use GetOwnerSVGElement() as it relies on GetParent(). This code is
   * basically a simplified version of GetOwnerSVGElement that uses the parent
   * parameters passed in instead.
   *
   * FIXME(bug 1596690): GetOwnerSVGElement() uses the flattened tree parent
   * rather than the DOM tree parent nowadays.
   */
  bool WillBeOutermostSVG(nsINode& aParent) const;

  LengthPercentage GetIntrinsicWidthOrHeight(int aAttr);

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

  const SVGAnimatedViewBox& GetViewBoxInternal() const override;
  SVGAnimatedTransformList* GetTransformInternal() const override;

  EnumAttributesInfo GetEnumInfo() override;

  enum { ZOOMANDPAN };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  // The time container for animations within this SVG document fragment. Set
  // for all outermost <svg> elements (not nested <svg> elements).
  UniquePtr<SMILTimeContainer> mTimedDocumentRoot;

  SVGPoint mCurrentTranslate;
  float mCurrentScale;

  // For outermost <svg> elements created from parsing, animation is started by
  // the onload event in accordance with the SVG spec, but for <svg> elements
  // created by script or promoted from inner <svg> to outermost <svg> we need
  // to manually kick off animation when they are bound to the tree.
  bool mStartAnimationOnBindToTree;

  bool mImageNeedsTransformInvalidation;

  // mCurrentViewID and mSVGView are mutually exclusive; we can have
  // at most one non-null.
  UniquePtr<nsString> mCurrentViewID;
  UniquePtr<SVGView> mSVGView;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGSVGElement, MOZILLA_SVGSVGELEMENT_IID)

}  // namespace dom

class MOZ_RAII AutoSVGTimeSetRestore {
 public:
  AutoSVGTimeSetRestore(dom::SVGSVGElement* aRootElem, float aFrameTime)
      : mRootElem(aRootElem),
        mOriginalTime(mRootElem->GetCurrentTimeAsFloat()) {
    mRootElem->SetCurrentTime(
        aFrameTime);  // Does nothing if there's no change.
  }

  ~AutoSVGTimeSetRestore() { mRootElem->SetCurrentTime(mOriginalTime); }

 private:
  const RefPtr<dom::SVGSVGElement> mRootElem;
  const float mOriginalTime;
};

class MOZ_RAII AutoPreserveAspectRatioOverride {
 public:
  AutoPreserveAspectRatioOverride(const SVGImageContext& aSVGContext,
                                  dom::SVGSVGElement* aRootElem)
      : mRootElem(aRootElem), mDidOverride(false) {
    MOZ_ASSERT(mRootElem, "No SVG/Symbol node to manage?");

    if (aSVGContext.GetPreserveAspectRatio().isSome()) {
      // Override preserveAspectRatio in our helper document.
      // XXXdholbert We should technically be overriding the helper doc's clip
      // and overflow properties here, too. See bug 272288 comment 36.
      mRootElem->SetImageOverridePreserveAspectRatio(
          *aSVGContext.GetPreserveAspectRatio());
      mDidOverride = true;
    }
  }

  ~AutoPreserveAspectRatioOverride() {
    if (mDidOverride) {
      mRootElem->ClearImageOverridePreserveAspectRatio();
    }
  }

 private:
  const RefPtr<dom::SVGSVGElement> mRootElem;
  bool mDidOverride;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGSVGELEMENT_H_
