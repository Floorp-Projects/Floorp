/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGELEMENT_H__
#define __NS_SVGELEMENT_H__

/*
  nsSVGElement is the base class for all SVG content elements.
  It implements all the common DOM interfaces and handles attributes.
*/

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsChangeHint.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/Element.h"
#include "nsISupportsImpl.h"
#include "nsStyledElement.h"
#include "nsSVGClass.h"
#include "nsIDOMSVGElement.h"
#include "SVGContentUtils.h"

class nsSVGAngle;
class nsSVGBoolean;
class nsSVGEnum;
class nsSVGInteger;
class nsSVGIntegerPair;
class nsSVGLength2;
class nsSVGNumber2;
class nsSVGNumberPair;
class nsSVGString;
class nsSVGViewBox;

namespace mozilla {
class DeclarationBlock;

namespace dom {
class SVGSVGElement;

static const unsigned short SVG_UNIT_TYPE_UNKNOWN           = 0;
static const unsigned short SVG_UNIT_TYPE_USERSPACEONUSE    = 1;
static const unsigned short SVG_UNIT_TYPE_OBJECTBOUNDINGBOX = 2;

} // namespace dom

class SVGAnimatedNumberList;
class SVGNumberList;
class SVGAnimatedLengthList;
class SVGUserUnitList;
class SVGAnimatedPointList;
class SVGAnimatedPathSegList;
class SVGAnimatedPreserveAspectRatio;
class nsSVGAnimatedTransformList;
class SVGStringList;
class DOMSVGStringList;

namespace gfx {
class Matrix;
} // namespace gfx

} // namespace mozilla

class gfxMatrix;
struct nsSVGEnumMapping;

typedef nsStyledElement nsSVGElementBase;

class nsSVGElement : public nsSVGElementBase    // nsIContent
                   , public nsIDOMSVGElement
{
protected:
  explicit nsSVGElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  friend nsresult NS_NewSVGElement(mozilla::dom::Element **aResult,
                                   already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  nsresult Init();
  virtual ~nsSVGElement();

public:

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_MUST_OVERRIDE override;

  typedef mozilla::SVGNumberList SVGNumberList;
  typedef mozilla::SVGAnimatedNumberList SVGAnimatedNumberList;
  typedef mozilla::SVGUserUnitList SVGUserUnitList;
  typedef mozilla::SVGAnimatedLengthList SVGAnimatedLengthList;
  typedef mozilla::SVGAnimatedPointList SVGAnimatedPointList;
  typedef mozilla::SVGAnimatedPathSegList SVGAnimatedPathSegList;
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;
  typedef mozilla::nsSVGAnimatedTransformList nsSVGAnimatedTransformList;
  typedef mozilla::SVGStringList SVGStringList;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  void DidAnimateClass();

  // nsIContent interface methods

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) override;
  void WalkAnimatedContentStyleRules(nsRuleWalker* aRuleWalker);

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  static const MappedAttributeEntry sFillStrokeMap[];
  static const MappedAttributeEntry sGraphicsMap[];
  static const MappedAttributeEntry sTextContentElementsMap[];
  static const MappedAttributeEntry sFontSpecificationMap[];
  static const MappedAttributeEntry sGradientStopMap[];
  static const MappedAttributeEntry sViewportsMap[];
  static const MappedAttributeEntry sMarkersMap[];
  static const MappedAttributeEntry sColorMap[];
  static const MappedAttributeEntry sFiltersMap[];
  static const MappedAttributeEntry sFEFloodMap[];
  static const MappedAttributeEntry sLightingEffectsMap[];
  static const MappedAttributeEntry sMaskMap[];

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_DECL_NSIDOMSVGELEMENT

  // Gets the element that establishes the rectangular viewport against which
  // we should resolve percentage lengths (our "coordinate context"). Returns
  // nullptr for outer <svg> or SVG without an <svg> parent (invalid SVG).
  mozilla::dom::SVGSVGElement* GetCtx() const;

  /**
   * Returns aMatrix pre-multiplied by (explicit or implicit) transforms that
   * are introduced by attributes on this element.
   *
   * If aWhich is eAllTransforms, then all the transforms from the coordinate
   * space established by this element for its children to the coordinate
   * space established by this element's parent element for this element, are
   * included.
   *
   * If aWhich is eUserSpaceToParent, then only the transforms from this
   * element's userspace to the coordinate space established by its parent is
   * included. This includes any transforms introduced by the 'transform'
   * attribute, transform animations and animateMotion, but not any offsets
   * due to e.g. 'x'/'y' attributes, or any transform due to a 'viewBox'
   * attribute. (SVG userspace is defined to be the coordinate space in which
   * coordinates on an element apply.)
   *
   * If aWhich is eChildToUserSpace, then only the transforms from the
   * coordinate space established by this element for its childre to this
   * elements userspace are included. This includes any offsets due to e.g.
   * 'x'/'y' attributes, and any transform due to a 'viewBox' attribute, but
   * does not include any transforms due to the 'transform' attribute.
   */
  virtual gfxMatrix PrependLocalTransformsTo(
    const gfxMatrix &aMatrix, SVGTransformTypes aWhich = eAllTransforms) const;

  // Setter for to set the current <animateMotion> transformation
  // Only visible for nsSVGGraphicElement, so it's a no-op here, and that
  // subclass has the useful implementation.
  virtual void SetAnimateMotionTransform(const mozilla::gfx::Matrix* aMatrix) {/*no-op*/}
  virtual const mozilla::gfx::Matrix* GetAnimateMotionTransform() const { return nullptr; }

  bool IsStringAnimatable(uint8_t aAttrEnum) {
    return GetStringInfo().mStringInfo[aAttrEnum].mIsAnimatable;
  }
  bool NumberAttrAllowsPercentage(uint8_t aAttrEnum) {
    return GetNumberInfo().mNumberInfo[aAttrEnum].mPercentagesAllowed;
  }
  virtual bool HasValidDimensions() const {
    return true;
  }
  void SetLength(nsIAtom* aName, const nsSVGLength2 &aLength);

  nsAttrValue WillChangeLength(uint8_t aAttrEnum);
  nsAttrValue WillChangeNumberPair(uint8_t aAttrEnum);
  nsAttrValue WillChangeIntegerPair(uint8_t aAttrEnum);
  nsAttrValue WillChangeAngle(uint8_t aAttrEnum);
  nsAttrValue WillChangeViewBox();
  nsAttrValue WillChangePreserveAspectRatio();
  nsAttrValue WillChangeNumberList(uint8_t aAttrEnum);
  nsAttrValue WillChangeLengthList(uint8_t aAttrEnum);
  nsAttrValue WillChangePointList();
  nsAttrValue WillChangePathSegList();
  nsAttrValue WillChangeTransformList();
  nsAttrValue WillChangeStringList(bool aIsConditionalProcessingAttribute,
                                   uint8_t aAttrEnum);

  void DidChangeLength(uint8_t aAttrEnum, const nsAttrValue& aEmptyOrOldValue);
  void DidChangeNumber(uint8_t aAttrEnum);
  void DidChangeNumberPair(uint8_t aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);
  void DidChangeInteger(uint8_t aAttrEnum);
  void DidChangeIntegerPair(uint8_t aAttrEnum,
                            const nsAttrValue& aEmptyOrOldValue);
  void DidChangeAngle(uint8_t aAttrEnum, const nsAttrValue& aEmptyOrOldValue);
  void DidChangeBoolean(uint8_t aAttrEnum);
  void DidChangeEnum(uint8_t aAttrEnum);
  void DidChangeViewBox(const nsAttrValue& aEmptyOrOldValue);
  void DidChangePreserveAspectRatio(const nsAttrValue& aEmptyOrOldValue);
  void DidChangeNumberList(uint8_t aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);
  void DidChangeLengthList(uint8_t aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);
  void DidChangePointList(const nsAttrValue& aEmptyOrOldValue);
  void DidChangePathSegList(const nsAttrValue& aEmptyOrOldValue);
  void DidChangeTransformList(const nsAttrValue& aEmptyOrOldValue);
  void DidChangeString(uint8_t aAttrEnum) {}
  void DidChangeStringList(bool aIsConditionalProcessingAttribute,
                           uint8_t aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);

  void DidAnimateLength(uint8_t aAttrEnum);
  void DidAnimateNumber(uint8_t aAttrEnum);
  void DidAnimateNumberPair(uint8_t aAttrEnum);
  void DidAnimateInteger(uint8_t aAttrEnum);
  void DidAnimateIntegerPair(uint8_t aAttrEnum);
  void DidAnimateAngle(uint8_t aAttrEnum);
  void DidAnimateBoolean(uint8_t aAttrEnum);
  void DidAnimateEnum(uint8_t aAttrEnum);
  void DidAnimateViewBox();
  void DidAnimatePreserveAspectRatio();
  void DidAnimateNumberList(uint8_t aAttrEnum);
  void DidAnimateLengthList(uint8_t aAttrEnum);
  void DidAnimatePointList();
  void DidAnimatePathSegList();
  void DidAnimateTransformList(int32_t aModType);
  void DidAnimateString(uint8_t aAttrEnum);

  enum {
    /**
     * Flag to indicate to GetAnimatedXxx() methods that the object being
     * requested should be allocated if it hasn't already been allocated, and
     * that the method should not return null. Only applicable to methods that
     * need to allocate the object that they return.
     */
    DO_ALLOCATE = 0x1
  };

  nsSVGLength2* GetAnimatedLength(const nsIAtom *aAttrName);
  void GetAnimatedLengthValues(float *aFirst, ...);
  void GetAnimatedNumberValues(float *aFirst, ...);
  void GetAnimatedIntegerValues(int32_t *aFirst, ...);
  SVGAnimatedNumberList* GetAnimatedNumberList(uint8_t aAttrEnum);
  SVGAnimatedNumberList* GetAnimatedNumberList(nsIAtom *aAttrName);
  void GetAnimatedLengthListValues(SVGUserUnitList *aFirst, ...);
  SVGAnimatedLengthList* GetAnimatedLengthList(uint8_t aAttrEnum);
  virtual SVGAnimatedPointList* GetAnimatedPointList() {
    return nullptr;
  }
  virtual SVGAnimatedPathSegList* GetAnimPathSegList() {
    // DOM interface 'SVGAnimatedPathData' (*inherited* by nsSVGPathElement)
    // has a member called 'animatedPathSegList' member, so we have a shorter
    // name so we don't get hidden by the GetAnimatedPathSegList declared by
    // NS_DECL_NSIDOMSVGANIMATEDPATHDATA.
    return nullptr;
  }
  /**
   * Get the nsSVGAnimatedTransformList for this element.
   *
   * Despite the fact that animated transform lists are used for a variety of
   * attributes, no SVG element uses more than one.
   *
   * It's relatively uncommon for elements to have their transform attribute
   * set, so to save memory the nsSVGAnimatedTransformList is not allocated until
   * the attribute is set/animated or its DOM wrapper is created. Callers that
   * require the nsSVGAnimatedTransformList to be allocated and for this method
   * to return non-null must pass the DO_ALLOCATE flag.
   */
  virtual nsSVGAnimatedTransformList* GetAnimatedTransformList(
                                                        uint32_t aFlags = 0) {
    return nullptr;
  }

  virtual nsISMILAttr* GetAnimatedAttr(int32_t aNamespaceID, nsIAtom* aName) override;
  void AnimationNeedsResample();
  void FlushAnimations();

  virtual void RecompileScriptEventListeners() override;

  void GetStringBaseValue(uint8_t aAttrEnum, nsAString& aResult) const;
  void SetStringBaseValue(uint8_t aAttrEnum, const nsAString& aValue);

  virtual nsIAtom* GetPointListAttrName() const {
    return nullptr;
  }
  virtual nsIAtom* GetPathDataAttrName() const {
    return nullptr;
  }
  virtual nsIAtom* GetTransformListAttrName() const {
    return nullptr;
  }
  const nsAttrValue* GetAnimatedClassName() const
  {
    if (!mClassAttribute.IsAnimated()) {
      return nullptr;
    }
    return mClassAnimAttr;
  }

  virtual void ClearAnyCachedPath() {}
  virtual nsIDOMNode* AsDOMNode() final override { return this; }
  virtual bool IsTransformable() { return false; }

  // WebIDL
  mozilla::dom::SVGSVGElement* GetOwnerSVGElement();
  nsSVGElement* GetViewportElement();
  already_AddRefed<mozilla::dom::SVGAnimatedString> ClassName();
  virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;

protected:
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

#ifdef DEBUG
  // We define BeforeSetAttr here and mark it final to ensure it is NOT used
  // by SVG elements.
  // This is because we're not currently passing the correct value for aValue to
  // BeforeSetAttr since it would involve allocating extra SVG value types.
  // See the comment in nsSVGElement::WillChangeValue.
  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 nsAttrValueOrString* aValue,
                                 bool aNotify) override final
  {
    return nsSVGElementBase::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
  }
#endif // DEBUG
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult) override;
  static nsresult ReportAttributeParseFailure(nsIDocument* aDocument,
                                              nsIAtom* aAttribute,
                                              const nsAString& aValue);

  void UpdateContentDeclarationBlock();
  void UpdateAnimatedContentDeclarationBlock();
  mozilla::DeclarationBlock* GetAnimatedContentDeclarationBlock();

  nsAttrValue WillChangeValue(nsIAtom* aName);
  // aNewValue is set to the old value. This value may be invalid if
  // !StoresOwnData.
  void DidChangeValue(nsIAtom* aName, const nsAttrValue& aEmptyOrOldValue,
                      nsAttrValue& aNewValue);
  void MaybeSerializeAttrBeforeRemoval(nsIAtom* aName, bool aNotify);

  static nsIAtom* GetEventNameForAttr(nsIAtom* aAttr);

  struct LengthInfo {
    nsIAtom** mName;
    float     mDefaultValue;
    uint8_t   mDefaultUnitType;
    uint8_t   mCtxType;
  };

  struct LengthAttributesInfo {
    nsSVGLength2* mLengths;
    LengthInfo*   mLengthInfo;
    uint32_t      mLengthCount;

    LengthAttributesInfo(nsSVGLength2 *aLengths,
                         LengthInfo *aLengthInfo,
                         uint32_t aLengthCount) :
      mLengths(aLengths), mLengthInfo(aLengthInfo), mLengthCount(aLengthCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct NumberInfo {
    nsIAtom** mName;
    float     mDefaultValue;
    bool mPercentagesAllowed;
  };

  struct NumberAttributesInfo {
    nsSVGNumber2* mNumbers;
    NumberInfo*   mNumberInfo;
    uint32_t      mNumberCount;

    NumberAttributesInfo(nsSVGNumber2 *aNumbers,
                         NumberInfo *aNumberInfo,
                         uint32_t aNumberCount) :
      mNumbers(aNumbers), mNumberInfo(aNumberInfo), mNumberCount(aNumberCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct NumberPairInfo {
    nsIAtom** mName;
    float     mDefaultValue1;
    float     mDefaultValue2;
  };

  struct NumberPairAttributesInfo {
    nsSVGNumberPair* mNumberPairs;
    NumberPairInfo*  mNumberPairInfo;
    uint32_t         mNumberPairCount;

    NumberPairAttributesInfo(nsSVGNumberPair *aNumberPairs,
                             NumberPairInfo *aNumberPairInfo,
                             uint32_t aNumberPairCount) :
      mNumberPairs(aNumberPairs), mNumberPairInfo(aNumberPairInfo),
      mNumberPairCount(aNumberPairCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct IntegerInfo {
    nsIAtom** mName;
    int32_t   mDefaultValue;
  };

  struct IntegerAttributesInfo {
    nsSVGInteger* mIntegers;
    IntegerInfo*  mIntegerInfo;
    uint32_t      mIntegerCount;

    IntegerAttributesInfo(nsSVGInteger *aIntegers,
                          IntegerInfo *aIntegerInfo,
                          uint32_t aIntegerCount) :
      mIntegers(aIntegers), mIntegerInfo(aIntegerInfo), mIntegerCount(aIntegerCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct IntegerPairInfo {
    nsIAtom** mName;
    int32_t   mDefaultValue1;
    int32_t   mDefaultValue2;
  };

  struct IntegerPairAttributesInfo {
    nsSVGIntegerPair* mIntegerPairs;
    IntegerPairInfo*  mIntegerPairInfo;
    uint32_t          mIntegerPairCount;

    IntegerPairAttributesInfo(nsSVGIntegerPair *aIntegerPairs,
                              IntegerPairInfo *aIntegerPairInfo,
                              uint32_t aIntegerPairCount) :
      mIntegerPairs(aIntegerPairs), mIntegerPairInfo(aIntegerPairInfo),
      mIntegerPairCount(aIntegerPairCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct AngleInfo {
    nsIAtom** mName;
    float     mDefaultValue;
    uint8_t   mDefaultUnitType;
  };

  struct AngleAttributesInfo {
    nsSVGAngle* mAngles;
    AngleInfo*  mAngleInfo;
    uint32_t    mAngleCount;

    AngleAttributesInfo(nsSVGAngle *aAngles,
                        AngleInfo *aAngleInfo,
                        uint32_t aAngleCount) :
      mAngles(aAngles), mAngleInfo(aAngleInfo), mAngleCount(aAngleCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct BooleanInfo {
    nsIAtom**    mName;
    bool mDefaultValue;
  };

  struct BooleanAttributesInfo {
    nsSVGBoolean* mBooleans;
    BooleanInfo*  mBooleanInfo;
    uint32_t      mBooleanCount;

    BooleanAttributesInfo(nsSVGBoolean *aBooleans,
                          BooleanInfo *aBooleanInfo,
                          uint32_t aBooleanCount) :
      mBooleans(aBooleans), mBooleanInfo(aBooleanInfo), mBooleanCount(aBooleanCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  friend class nsSVGEnum;

  struct EnumInfo {
    nsIAtom**         mName;
    nsSVGEnumMapping* mMapping;
    uint16_t          mDefaultValue;
  };

  struct EnumAttributesInfo {
    nsSVGEnum* mEnums;
    EnumInfo*  mEnumInfo;
    uint32_t   mEnumCount;

    EnumAttributesInfo(nsSVGEnum *aEnums,
                       EnumInfo *aEnumInfo,
                       uint32_t aEnumCount) :
      mEnums(aEnums), mEnumInfo(aEnumInfo), mEnumCount(aEnumCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  struct NumberListInfo {
    nsIAtom** mName;
  };

  struct NumberListAttributesInfo {
    SVGAnimatedNumberList* mNumberLists;
    NumberListInfo*        mNumberListInfo;
    uint32_t               mNumberListCount;

    NumberListAttributesInfo(SVGAnimatedNumberList *aNumberLists,
                             NumberListInfo *aNumberListInfo,
                             uint32_t aNumberListCount)
      : mNumberLists(aNumberLists)
      , mNumberListInfo(aNumberListInfo)
      , mNumberListCount(aNumberListCount)
    {}

    void Reset(uint8_t aAttrEnum);
  };

  struct LengthListInfo {
    nsIAtom** mName;
    uint8_t   mAxis;
    /**
     * Flag to indicate whether appending zeros to the end of the list would
     * change the rendering of the SVG for the attribute in question. For x and
     * y on the <text> element this is true, but for dx and dy on <text> this
     * is false. This flag is fed down to SVGLengthListSMILType so it can
     * determine if it can sensibly animate from-to lists of different lengths,
     * which is desirable in the case of dx and dy.
     */
    bool mCouldZeroPadList;
  };

  struct LengthListAttributesInfo {
    SVGAnimatedLengthList* mLengthLists;
    LengthListInfo*        mLengthListInfo;
    uint32_t               mLengthListCount;

    LengthListAttributesInfo(SVGAnimatedLengthList *aLengthLists,
                             LengthListInfo *aLengthListInfo,
                             uint32_t aLengthListCount)
      : mLengthLists(aLengthLists)
      , mLengthListInfo(aLengthListInfo)
      , mLengthListCount(aLengthListCount)
    {}

    void Reset(uint8_t aAttrEnum);
  };

  struct StringInfo {
    nsIAtom**    mName;
    int32_t      mNamespaceID;
    bool mIsAnimatable;
  };

  struct StringAttributesInfo {
    nsSVGString*  mStrings;
    StringInfo*   mStringInfo;
    uint32_t      mStringCount;

    StringAttributesInfo(nsSVGString *aStrings,
                         StringInfo *aStringInfo,
                         uint32_t aStringCount) :
      mStrings(aStrings), mStringInfo(aStringInfo), mStringCount(aStringCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  friend class mozilla::DOMSVGStringList;

  struct StringListInfo {
    nsIAtom**    mName;
  };

  struct StringListAttributesInfo {
    SVGStringList*    mStringLists;
    StringListInfo*   mStringListInfo;
    uint32_t          mStringListCount;

    StringListAttributesInfo(SVGStringList  *aStringLists,
                             StringListInfo *aStringListInfo,
                             uint32_t aStringListCount) :
      mStringLists(aStringLists), mStringListInfo(aStringListInfo),
      mStringListCount(aStringListCount)
      {}

    void Reset(uint8_t aAttrEnum);
  };

  virtual LengthAttributesInfo GetLengthInfo();
  virtual NumberAttributesInfo GetNumberInfo();
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual IntegerPairAttributesInfo GetIntegerPairInfo();
  virtual AngleAttributesInfo GetAngleInfo();
  virtual BooleanAttributesInfo GetBooleanInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  // We assume all viewboxes and preserveAspectRatios are alike
  // so we don't need to wrap the class
  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();
  virtual NumberListAttributesInfo GetNumberListInfo();
  virtual LengthListAttributesInfo GetLengthListInfo();
  virtual StringAttributesInfo GetStringInfo();
  virtual StringListAttributesInfo GetStringListInfo();

  static nsSVGEnumMapping sSVGUnitTypesMap[];

private:
  void UnsetAttrInternal(int32_t aNameSpaceID, nsIAtom* aAttribute,
                         bool aNotify);

  nsSVGClass mClassAttribute;
  nsAutoPtr<nsAttrValue> mClassAnimAttr;
  RefPtr<mozilla::DeclarationBlock> mContentDeclarationBlock;
};

/**
 * A macro to implement the NS_NewSVGXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_SVG_ELEMENT(_elementName)                             \
nsresult                                                                     \
NS_NewSVG##_elementName##Element(nsIContent **aResult,                       \
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)  \
{                                                                            \
  RefPtr<nsSVG##_elementName##Element> it =                                \
    new nsSVG##_elementName##Element(aNodeInfo);                             \
                                                                             \
  nsresult rv = it->Init();                                                  \
                                                                             \
  if (NS_FAILED(rv)) {                                                       \
    return rv;                                                               \
  }                                                                          \
                                                                             \
  it.forget(aResult);                                                        \
                                                                             \
  return rv;                                                                 \
}

#define NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(_elementName)                  \
nsresult                                                                     \
NS_NewSVG##_elementName##Element(nsIContent **aResult,                       \
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)  \
{                                                                            \
  RefPtr<mozilla::dom::SVG##_elementName##Element> it =                    \
    new mozilla::dom::SVG##_elementName##Element(aNodeInfo);                 \
                                                                             \
  nsresult rv = it->Init();                                                  \
                                                                             \
  if (NS_FAILED(rv)) {                                                       \
    return rv;                                                               \
  }                                                                          \
                                                                             \
  it.forget(aResult);                                                        \
                                                                             \
  return rv;                                                                 \
}

#define NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT_CHECK_PARSER(_elementName)     \
nsresult                                                                     \
NS_NewSVG##_elementName##Element(nsIContent **aResult,                       \
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,  \
                                 mozilla::dom::FromParser aFromParser)       \
{                                                                            \
  RefPtr<mozilla::dom::SVG##_elementName##Element> it =                    \
    new mozilla::dom::SVG##_elementName##Element(aNodeInfo, aFromParser);    \
                                                                             \
  nsresult rv = it->Init();                                                  \
                                                                             \
  if (NS_FAILED(rv)) {                                                       \
    return rv;                                                               \
  }                                                                          \
                                                                             \
  it.forget(aResult);                                                        \
                                                                             \
  return rv;                                                                 \
}

// No unlinking, we'd need to null out the value pointer (the object it
// points to is held by the element) and null-check it everywhere.
#define NS_SVG_VAL_IMPL_CYCLE_COLLECTION(_val, _element)                     \
NS_IMPL_CYCLE_COLLECTION_CLASS(_val)                                         \
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_val)                                \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_element) \
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                        \
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(_val)

#define NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(_val, _element)       \
NS_IMPL_CYCLE_COLLECTION_CLASS(_val)                                         \
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_val)                                  \
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER                            \
NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                          \
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_val)                                \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_element)                                \
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                        \
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_val)                                   \
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER                             \
NS_IMPL_CYCLE_COLLECTION_TRACE_END

#endif // __NS_SVGELEMENT_H__
