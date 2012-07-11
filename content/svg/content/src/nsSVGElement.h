/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGELEMENT_H__
#define __NS_SVGELEMENT_H__

/*
  nsSVGElement is the base class for all SVG content elements.
  It implements all the common DOM interfaces and handles attributes.
*/

#include "mozilla/css/StyleRule.h"
#include "nsAutoPtr.h"
#include "nsChangeHint.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsGenericElement.h"
#include "nsISupportsImpl.h"
#include "nsStyledElement.h"

class nsIDOMSVGElement;
class nsIDOMSVGSVGElement;
class nsSVGAngle;
class nsSVGBoolean;
class nsSVGEnum;
class nsSVGInteger;
class nsSVGIntegerPair;
class nsSVGLength2;
class nsSVGNumber2;
class nsSVGNumberPair;
class nsSVGString;
class nsSVGSVGElement;
class nsSVGViewBox;

namespace mozilla {
class SVGAnimatedNumberList;
class SVGNumberList;
class SVGAnimatedLengthList;
class SVGUserUnitList;
class SVGAnimatedPointList;
class SVGAnimatedPathSegList;
class SVGAnimatedPreserveAspectRatio;
class SVGAnimatedTransformList;
class SVGStringList;
class DOMSVGStringList;
}

struct gfxMatrix;
struct nsSVGEnumMapping;

typedef nsStyledElementNotElementCSSInlineStyle nsSVGElementBase;

class nsSVGElement : public nsSVGElementBase    // nsIContent
{
protected:
  nsSVGElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();
  virtual ~nsSVGElement(){}

public:
  typedef mozilla::SVGNumberList SVGNumberList;
  typedef mozilla::SVGAnimatedNumberList SVGAnimatedNumberList;
  typedef mozilla::SVGUserUnitList SVGUserUnitList;
  typedef mozilla::SVGAnimatedLengthList SVGAnimatedLengthList;
  typedef mozilla::SVGAnimatedPointList SVGAnimatedPointList;
  typedef mozilla::SVGAnimatedPathSegList SVGAnimatedPathSegList;
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;
  typedef mozilla::SVGAnimatedTransformList SVGAnimatedTransformList;
  typedef mozilla::SVGStringList SVGStringList;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface methods

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);

  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;

  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

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

  // nsIDOMNode
  NS_IMETHOD IsSupported(const nsAString& aFeature, const nsAString& aVersion,
                         bool* aReturn);
  
  // nsIDOMSVGElement
  NS_IMETHOD GetId(nsAString & aId);
  NS_IMETHOD SetId(const nsAString & aId);
  NS_IMETHOD GetOwnerSVGElement(nsIDOMSVGSVGElement** aOwnerSVGElement);
  NS_IMETHOD GetViewportElement(nsIDOMSVGElement** aViewportElement);

  // Gets the element that establishes the rectangular viewport against which
  // we should resolve percentage lengths (our "coordinate context"). Returns
  // nsnull for outer <svg> or SVG without an <svg> parent (invalid SVG).
  nsSVGSVGElement* GetCtx() const;

  enum TransformTypes {
     eAllTransforms
    ,eUserSpaceToParent
    ,eChildToUserSpace
  };
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
  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;

  // Setter for to set the current <animateMotion> transformation
  // Only visible for nsSVGGraphicElement, so it's a no-op here, and that
  // subclass has the useful implementation.
  virtual void SetAnimateMotionTransform(const gfxMatrix* aMatrix) {/*no-op*/}
  virtual const gfxMatrix* GetAnimateMotionTransform() const { return nsnull; }

  bool IsStringAnimatable(PRUint8 aAttrEnum) {
    return GetStringInfo().mStringInfo[aAttrEnum].mIsAnimatable;
  }
  bool NumberAttrAllowsPercentage(PRUint8 aAttrEnum) {
    return GetNumberInfo().mNumberInfo[aAttrEnum].mPercentagesAllowed;
  }
  virtual bool HasValidDimensions() const {
    return true;
  }
  void SetLength(nsIAtom* aName, const nsSVGLength2 &aLength);

  nsAttrValue WillChangeLength(PRUint8 aAttrEnum);
  nsAttrValue WillChangeNumberPair(PRUint8 aAttrEnum);
  nsAttrValue WillChangeIntegerPair(PRUint8 aAttrEnum);
  nsAttrValue WillChangeAngle(PRUint8 aAttrEnum);
  nsAttrValue WillChangeViewBox();
  nsAttrValue WillChangePreserveAspectRatio();
  nsAttrValue WillChangeNumberList(PRUint8 aAttrEnum);
  nsAttrValue WillChangeLengthList(PRUint8 aAttrEnum);
  nsAttrValue WillChangePointList();
  nsAttrValue WillChangePathSegList();
  nsAttrValue WillChangeTransformList();
  nsAttrValue WillChangeStringList(bool aIsConditionalProcessingAttribute,
                                   PRUint8 aAttrEnum);

  void DidChangeLength(PRUint8 aAttrEnum, const nsAttrValue& aEmptyOrOldValue);
  void DidChangeNumber(PRUint8 aAttrEnum);
  void DidChangeNumberPair(PRUint8 aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);
  void DidChangeInteger(PRUint8 aAttrEnum);
  void DidChangeIntegerPair(PRUint8 aAttrEnum,
                            const nsAttrValue& aEmptyOrOldValue);
  void DidChangeAngle(PRUint8 aAttrEnum, const nsAttrValue& aEmptyOrOldValue);
  void DidChangeBoolean(PRUint8 aAttrEnum);
  void DidChangeEnum(PRUint8 aAttrEnum);
  void DidChangeViewBox(const nsAttrValue& aEmptyOrOldValue);
  void DidChangePreserveAspectRatio(const nsAttrValue& aEmptyOrOldValue);
  void DidChangeNumberList(PRUint8 aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);
  void DidChangeLengthList(PRUint8 aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);
  void DidChangePointList(const nsAttrValue& aEmptyOrOldValue);
  void DidChangePathSegList(const nsAttrValue& aEmptyOrOldValue);
  void DidChangeTransformList(const nsAttrValue& aEmptyOrOldValue);
  void DidChangeString(PRUint8 aAttrEnum) {}
  void DidChangeStringList(bool aIsConditionalProcessingAttribute,
                           PRUint8 aAttrEnum,
                           const nsAttrValue& aEmptyOrOldValue);

  void DidAnimateLength(PRUint8 aAttrEnum);
  void DidAnimateNumber(PRUint8 aAttrEnum);
  void DidAnimateNumberPair(PRUint8 aAttrEnum);
  void DidAnimateInteger(PRUint8 aAttrEnum);
  void DidAnimateIntegerPair(PRUint8 aAttrEnum);
  void DidAnimateAngle(PRUint8 aAttrEnum);
  void DidAnimateBoolean(PRUint8 aAttrEnum);
  void DidAnimateEnum(PRUint8 aAttrEnum);
  void DidAnimateViewBox();
  void DidAnimatePreserveAspectRatio();
  void DidAnimateNumberList(PRUint8 aAttrEnum);
  void DidAnimateLengthList(PRUint8 aAttrEnum);
  void DidAnimatePointList();
  void DidAnimatePathSegList();
  void DidAnimateTransformList();
  void DidAnimateString(PRUint8 aAttrEnum);

  nsSVGLength2* GetAnimatedLength(const nsIAtom *aAttrName);
  void GetAnimatedLengthValues(float *aFirst, ...);
  void GetAnimatedNumberValues(float *aFirst, ...);
  void GetAnimatedIntegerValues(PRInt32 *aFirst, ...);
  SVGAnimatedNumberList* GetAnimatedNumberList(PRUint8 aAttrEnum);
  SVGAnimatedNumberList* GetAnimatedNumberList(nsIAtom *aAttrName);
  void GetAnimatedLengthListValues(SVGUserUnitList *aFirst, ...);
  SVGAnimatedLengthList* GetAnimatedLengthList(PRUint8 aAttrEnum);
  virtual SVGAnimatedPointList* GetAnimatedPointList() {
    return nsnull;
  }
  virtual SVGAnimatedPathSegList* GetAnimPathSegList() {
    // DOM interface 'SVGAnimatedPathData' (*inherited* by nsSVGPathElement)
    // has a member called 'animatedPathSegList' member, so we have a shorter
    // name so we don't get hidden by the GetAnimatedPathSegList declared by
    // NS_DECL_NSIDOMSVGANIMATEDPATHDATA.
    return nsnull;
  }
  // Despite the fact that animated transform lists are used for a variety of
  // attributes, no SVG element uses more than one.
  virtual SVGAnimatedTransformList* GetAnimatedTransformList() {
    return nsnull;
  }

  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 aNamespaceID, nsIAtom* aName);
  void AnimationNeedsResample();
  void FlushAnimations();

  virtual void RecompileScriptEventListeners();

  void GetStringBaseValue(PRUint8 aAttrEnum, nsAString& aResult) const;
  void SetStringBaseValue(PRUint8 aAttrEnum, const nsAString& aValue);

  virtual nsIAtom* GetPointListAttrName() const {
    return nsnull;
  }
  virtual nsIAtom* GetPathDataAttrName() const {
    return nsnull;
  }
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsnull;
  }

protected:
#ifdef DEBUG
  // We define BeforeSetAttr here and mark it MOZ_FINAL to ensure it is NOT used
  // by SVG elements.
  // This is because we're not currently passing the correct value for aValue to
  // BeforeSetAttr since it would involve allocating extra SVG value types.
  // See the comment in nsSVGElement::WillChangeValue.
  virtual nsresult BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) MOZ_FINAL { return NS_OK; }
#endif // DEBUG
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);
  virtual bool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult);
  static nsresult ReportAttributeParseFailure(nsIDocument* aDocument,
                                              nsIAtom* aAttribute,
                                              const nsAString& aValue);

  // Hooks for subclasses
  virtual bool IsEventName(nsIAtom* aName);

  void UpdateContentStyleRule();
  void UpdateAnimatedContentStyleRule();
  mozilla::css::StyleRule* GetAnimatedContentStyleRule();

  nsAttrValue WillChangeValue(nsIAtom* aName);
  void DidChangeValue(nsIAtom* aName, const nsAttrValue& aEmptyOrOldValue,
                      nsAttrValue& aNewValue);
  void MaybeSerializeAttrBeforeRemoval(nsIAtom* aName, bool aNotify);

  static nsIAtom* GetEventNameForAttr(nsIAtom* aAttr);

  struct LengthInfo {
    nsIAtom** mName;
    float     mDefaultValue;
    PRUint8   mDefaultUnitType;
    PRUint8   mCtxType;
  };

  struct LengthAttributesInfo {
    nsSVGLength2* mLengths;
    LengthInfo*   mLengthInfo;
    PRUint32      mLengthCount;

    LengthAttributesInfo(nsSVGLength2 *aLengths,
                         LengthInfo *aLengthInfo,
                         PRUint32 aLengthCount) :
      mLengths(aLengths), mLengthInfo(aLengthInfo), mLengthCount(aLengthCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct NumberInfo {
    nsIAtom** mName;
    float     mDefaultValue;
    bool mPercentagesAllowed;
  };

  struct NumberAttributesInfo {
    nsSVGNumber2* mNumbers;
    NumberInfo*   mNumberInfo;
    PRUint32      mNumberCount;

    NumberAttributesInfo(nsSVGNumber2 *aNumbers,
                         NumberInfo *aNumberInfo,
                         PRUint32 aNumberCount) :
      mNumbers(aNumbers), mNumberInfo(aNumberInfo), mNumberCount(aNumberCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct NumberPairInfo {
    nsIAtom** mName;
    float     mDefaultValue1;
    float     mDefaultValue2;
  };

  struct NumberPairAttributesInfo {
    nsSVGNumberPair* mNumberPairs;
    NumberPairInfo*  mNumberPairInfo;
    PRUint32         mNumberPairCount;

    NumberPairAttributesInfo(nsSVGNumberPair *aNumberPairs,
                             NumberPairInfo *aNumberPairInfo,
                             PRUint32 aNumberPairCount) :
      mNumberPairs(aNumberPairs), mNumberPairInfo(aNumberPairInfo),
      mNumberPairCount(aNumberPairCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct IntegerInfo {
    nsIAtom** mName;
    PRInt32   mDefaultValue;
  };

  struct IntegerAttributesInfo {
    nsSVGInteger* mIntegers;
    IntegerInfo*  mIntegerInfo;
    PRUint32      mIntegerCount;

    IntegerAttributesInfo(nsSVGInteger *aIntegers,
                          IntegerInfo *aIntegerInfo,
                          PRUint32 aIntegerCount) :
      mIntegers(aIntegers), mIntegerInfo(aIntegerInfo), mIntegerCount(aIntegerCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct IntegerPairInfo {
    nsIAtom** mName;
    PRInt32   mDefaultValue1;
    PRInt32   mDefaultValue2;
  };

  struct IntegerPairAttributesInfo {
    nsSVGIntegerPair* mIntegerPairs;
    IntegerPairInfo*  mIntegerPairInfo;
    PRUint32          mIntegerPairCount;

    IntegerPairAttributesInfo(nsSVGIntegerPair *aIntegerPairs,
                              IntegerPairInfo *aIntegerPairInfo,
                              PRUint32 aIntegerPairCount) :
      mIntegerPairs(aIntegerPairs), mIntegerPairInfo(aIntegerPairInfo),
      mIntegerPairCount(aIntegerPairCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct AngleInfo {
    nsIAtom** mName;
    float     mDefaultValue;
    PRUint8   mDefaultUnitType;
  };

  struct AngleAttributesInfo {
    nsSVGAngle* mAngles;
    AngleInfo*  mAngleInfo;
    PRUint32    mAngleCount;

    AngleAttributesInfo(nsSVGAngle *aAngles,
                        AngleInfo *aAngleInfo,
                        PRUint32 aAngleCount) :
      mAngles(aAngles), mAngleInfo(aAngleInfo), mAngleCount(aAngleCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct BooleanInfo {
    nsIAtom**    mName;
    bool mDefaultValue;
  };

  struct BooleanAttributesInfo {
    nsSVGBoolean* mBooleans;
    BooleanInfo*  mBooleanInfo;
    PRUint32      mBooleanCount;

    BooleanAttributesInfo(nsSVGBoolean *aBooleans,
                          BooleanInfo *aBooleanInfo,
                          PRUint32 aBooleanCount) :
      mBooleans(aBooleans), mBooleanInfo(aBooleanInfo), mBooleanCount(aBooleanCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  friend class nsSVGEnum;

  struct EnumInfo {
    nsIAtom**         mName;
    nsSVGEnumMapping* mMapping;
    PRUint16          mDefaultValue;
  };

  struct EnumAttributesInfo {
    nsSVGEnum* mEnums;
    EnumInfo*  mEnumInfo;
    PRUint32   mEnumCount;

    EnumAttributesInfo(nsSVGEnum *aEnums,
                       EnumInfo *aEnumInfo,
                       PRUint32 aEnumCount) :
      mEnums(aEnums), mEnumInfo(aEnumInfo), mEnumCount(aEnumCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct NumberListInfo {
    nsIAtom** mName;
  };

  struct NumberListAttributesInfo {
    SVGAnimatedNumberList* mNumberLists;
    NumberListInfo*        mNumberListInfo;
    PRUint32               mNumberListCount;

    NumberListAttributesInfo(SVGAnimatedNumberList *aNumberLists,
                             NumberListInfo *aNumberListInfo,
                             PRUint32 aNumberListCount)
      : mNumberLists(aNumberLists)
      , mNumberListInfo(aNumberListInfo)
      , mNumberListCount(aNumberListCount)
    {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct LengthListInfo {
    nsIAtom** mName;
    PRUint8   mAxis;
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
    PRUint32               mLengthListCount;

    LengthListAttributesInfo(SVGAnimatedLengthList *aLengthLists,
                             LengthListInfo *aLengthListInfo,
                             PRUint32 aLengthListCount)
      : mLengthLists(aLengthLists)
      , mLengthListInfo(aLengthListInfo)
      , mLengthListCount(aLengthListCount)
    {}

    void Reset(PRUint8 aAttrEnum);
  };

  struct StringInfo {
    nsIAtom**    mName;
    PRInt32      mNamespaceID;
    bool mIsAnimatable;
  };

  struct StringAttributesInfo {
    nsSVGString*  mStrings;
    StringInfo*   mStringInfo;
    PRUint32      mStringCount;

    StringAttributesInfo(nsSVGString *aStrings,
                         StringInfo *aStringInfo,
                         PRUint32 aStringCount) :
      mStrings(aStrings), mStringInfo(aStringInfo), mStringCount(aStringCount)
      {}

    void Reset(PRUint8 aAttrEnum);
  };

  friend class mozilla::DOMSVGStringList;

  struct StringListInfo {
    nsIAtom**    mName;
  };

  struct StringListAttributesInfo {
    SVGStringList*    mStringLists;
    StringListInfo*   mStringListInfo;
    PRUint32          mStringListCount;

    StringListAttributesInfo(SVGStringList  *aStringLists,
                             StringListInfo *aStringListInfo,
                             PRUint32 aStringListCount) :
      mStringLists(aStringLists), mStringListInfo(aStringListInfo),
      mStringListCount(aStringListCount)
      {}

    void Reset(PRUint8 aAttrEnum);
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
  void UnsetAttrInternal(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                         bool aNotify);

  nsRefPtr<mozilla::css::StyleRule> mContentStyleRule;
};

/**
 * A macro to implement the NS_NewSVGXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_SVG_ELEMENT(_elementName)                             \
nsresult                                                                     \
NS_NewSVG##_elementName##Element(nsIContent **aResult,                       \
                                 already_AddRefed<nsINodeInfo> aNodeInfo)    \
{                                                                            \
  nsRefPtr<nsSVG##_elementName##Element> it =                                \
    new nsSVG##_elementName##Element(aNodeInfo);                             \
  if (!it)                                                                   \
    return NS_ERROR_OUT_OF_MEMORY;                                           \
                                                                             \
  nsresult rv = it->Init();                                                  \
                                                                             \
  if (NS_FAILED(rv)) {                                                       \
    return rv;                                                               \
  }                                                                          \
                                                                             \
  *aResult = it.forget().get();                                              \
                                                                             \
  return rv;                                                                 \
}

#define NS_IMPL_NS_NEW_SVG_ELEMENT_CHECK_PARSER(_elementName)                \
nsresult                                                                     \
NS_NewSVG##_elementName##Element(nsIContent **aResult,                       \
                                 already_AddRefed<nsINodeInfo> aNodeInfo,    \
                                 FromParser aFromParser)                     \
{                                                                            \
  nsRefPtr<nsSVG##_elementName##Element> it =                                \
    new nsSVG##_elementName##Element(aNodeInfo, aFromParser);                \
  if (!it)                                                                   \
    return NS_ERROR_OUT_OF_MEMORY;                                           \
                                                                             \
  nsresult rv = it->Init();                                                  \
                                                                             \
  if (NS_FAILED(rv)) {                                                       \
    return rv;                                                               \
  }                                                                          \
                                                                             \
  *aResult = it.forget().get();                                              \
                                                                             \
  return rv;                                                                 \
}

 // No unlinking, we'd need to null out the value pointer (the object it
// points to is held by the element) and null-check it everywhere.
#define NS_SVG_VAL_IMPL_CYCLE_COLLECTION(_val, _element)                     \
NS_IMPL_CYCLE_COLLECTION_CLASS(_val)                                         \
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_val)                                \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(_element, nsIContent) \
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                        \
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(_val)


#endif // __NS_SVGELEMENT_H__
