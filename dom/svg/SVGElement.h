/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGELEMENT_H__
#define __NS_SVGELEMENT_H__

/*
  SVGElement is the base class for all SVG content elements.
  It implements all the common DOM interfaces and handles attributes.
*/

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGAnimatedClass.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "nsAutoPtr.h"
#include "nsChangeHint.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsStyledElement.h"
#include "gfxMatrix.h"

nsresult NS_NewSVGElement(mozilla::dom::Element** aResult,
                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class DeclarationBlock;

class DOMSVGStringList;
class SVGAnimatedBoolean;
class SVGAnimatedEnumeration;
class SVGAnimatedInteger;
class SVGAnimatedIntegerPair;
class SVGAnimatedLength;
class SVGAnimatedLengthList;
class SVGAnimatedNumber;
class SVGAnimatedNumberList;
class SVGAnimatedNumberPair;
class SVGAnimatedOrient;
class SVGAnimatedPathSegList;
class SVGAnimatedPointList;
class SVGAnimatedString;
class SVGAnimatedPreserveAspectRatio;
class SVGAnimatedTransformList;
class SVGAnimatedViewBox;
class SVGNumberList;
class SVGStringList;
class SVGUserUnitList;

struct SVGEnumMapping;

namespace dom {
class SVGSVGElement;
class SVGViewportElement;

typedef nsStyledElement SVGElementBase;

class SVGElement : public SVGElementBase  // nsIContent
{
 protected:
  explicit SVGElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  friend nsresult(
      ::NS_NewSVGElement(mozilla::dom::Element** aResult,
                         already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  nsresult Init();
  virtual ~SVGElement();

 public:
  virtual nsresult Clone(mozilla::dom::NodeInfo*,
                         nsINode** aResult) const MOZ_MUST_OVERRIDE override;

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(SVGElement, SVGElementBase)

  void DidAnimateClass();

  // nsIContent interface methods

  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;

  virtual nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                              int32_t aModType) const override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  /**
   * We override the default to unschedule computation of Servo declaration
   * blocks when adopted across documents.
   */
  virtual void NodeInfoChanged(Document* aOldDoc) override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

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

  NS_IMPL_FROMNODE(SVGElement, kNameSpaceID_SVG)

  // Gets the element that establishes the rectangular viewport against which
  // we should resolve percentage lengths (our "coordinate context"). Returns
  // nullptr for outer <svg> or SVG without an <svg> parent (invalid SVG).
  mozilla::dom::SVGViewportElement* GetCtx() const;

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
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const;

  // Setter for to set the current <animateMotion> transformation
  // Only visible for nsSVGGraphicElement, so it's a no-op here, and that
  // subclass has the useful implementation.
  virtual void SetAnimateMotionTransform(
      const mozilla::gfx::Matrix* aMatrix) { /*no-op*/
  }
  virtual const mozilla::gfx::Matrix* GetAnimateMotionTransform() const {
    return nullptr;
  }

  bool IsStringAnimatable(uint8_t aAttrEnum) {
    return GetStringInfo().mStringInfo[aAttrEnum].mIsAnimatable;
  }
  bool NumberAttrAllowsPercentage(uint8_t aAttrEnum) {
    return GetNumberInfo().mNumberInfo[aAttrEnum].mPercentagesAllowed;
  }
  virtual bool HasValidDimensions() const { return true; }
  void SetLength(nsAtom* aName, const SVGAnimatedLength& aLength);

  nsAttrValue WillChangeLength(uint8_t aAttrEnum);
  nsAttrValue WillChangeNumberPair(uint8_t aAttrEnum);
  nsAttrValue WillChangeIntegerPair(uint8_t aAttrEnum);
  nsAttrValue WillChangeOrient();
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
  void DidChangeBoolean(uint8_t aAttrEnum);
  void DidChangeEnum(uint8_t aAttrEnum);
  void DidChangeOrient(const nsAttrValue& aEmptyOrOldValue);
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
  void DidAnimateBoolean(uint8_t aAttrEnum);
  void DidAnimateEnum(uint8_t aAttrEnum);
  void DidAnimateOrient();
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

  SVGAnimatedLength* GetAnimatedLength(const nsAtom* aAttrName);
  void GetAnimatedLengthValues(float* aFirst, ...);
  void GetAnimatedNumberValues(float* aFirst, ...);
  void GetAnimatedIntegerValues(int32_t* aFirst, ...);
  SVGAnimatedNumberList* GetAnimatedNumberList(uint8_t aAttrEnum);
  SVGAnimatedNumberList* GetAnimatedNumberList(nsAtom* aAttrName);
  void GetAnimatedLengthListValues(SVGUserUnitList* aFirst, ...);
  SVGAnimatedLengthList* GetAnimatedLengthList(uint8_t aAttrEnum);
  virtual SVGAnimatedPointList* GetAnimatedPointList() { return nullptr; }
  virtual SVGAnimatedPathSegList* GetAnimPathSegList() {
    // DOM interface 'SVGAnimatedPathData' (*inherited* by nsSVGPathElement)
    // has a member called 'animatedPathSegList' member, so we have a shorter
    // name so we don't get hidden by the GetAnimatedPathSegList declared by
    // NS_DECL_NSIDOMSVGANIMATEDPATHDATA.
    return nullptr;
  }
  /**
   * Get the SVGAnimatedTransformList for this element.
   *
   * Despite the fact that animated transform lists are used for a variety of
   * attributes, no SVG element uses more than one.
   *
   * It's relatively uncommon for elements to have their transform attribute
   * set, so to save memory the SVGAnimatedTransformList is not allocated
   * until the attribute is set/animated or its DOM wrapper is created. Callers
   * that require the SVGAnimatedTransformList to be allocated and for this
   * method to return non-null must pass the DO_ALLOCATE flag.
   */
  virtual SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) {
    return nullptr;
  }

  mozilla::UniquePtr<SMILAttr> GetAnimatedAttr(int32_t aNamespaceID,
                                               nsAtom* aName) override;
  void AnimationNeedsResample();
  void FlushAnimations();

  virtual void RecompileScriptEventListeners() override;

  void GetStringBaseValue(uint8_t aAttrEnum, nsAString& aResult) const;
  void SetStringBaseValue(uint8_t aAttrEnum, const nsAString& aValue);

  virtual nsStaticAtom* GetPointListAttrName() const { return nullptr; }
  virtual nsStaticAtom* GetPathDataAttrName() const { return nullptr; }
  virtual nsStaticAtom* GetTransformListAttrName() const { return nullptr; }
  const nsAttrValue* GetAnimatedClassName() const {
    if (!mClassAttribute.IsAnimated()) {
      return nullptr;
    }
    return mClassAnimAttr;
  }

  virtual void ClearAnyCachedPath() {}
  virtual bool IsTransformable() { return false; }

  // WebIDL
  mozilla::dom::SVGSVGElement* GetOwnerSVGElement();
  SVGElement* GetViewportElement();
  already_AddRefed<mozilla::dom::DOMSVGAnimatedString> ClassName();

  void UpdateContentDeclarationBlock();
  const mozilla::DeclarationBlock* GetContentDeclarationBlock() const;

 protected:
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

  // We define BeforeSetAttr here and mark it final to ensure it is NOT used
  // by SVG elements.
  // This is because we're not currently passing the correct value for aValue to
  // BeforeSetAttr since it would involve allocating extra SVG value types.
  // See the comment in SVGElement::WillChangeValue.
  nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                         const nsAttrValueOrString* aValue, bool aNotify) final;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  static nsresult ReportAttributeParseFailure(Document* aDocument,
                                              nsAtom* aAttribute,
                                              const nsAString& aValue);

  nsAttrValue WillChangeValue(nsAtom* aName);
  // aNewValue is set to the old value. This value may be invalid if
  // !StoresOwnData.
  void DidChangeValue(nsAtom* aName, const nsAttrValue& aEmptyOrOldValue,
                      nsAttrValue& aNewValue);
  void MaybeSerializeAttrBeforeRemoval(nsAtom* aName, bool aNotify);

  static nsAtom* GetEventNameForAttr(nsAtom* aAttr);

  struct LengthInfo {
    nsStaticAtom* const mName;
    const float mDefaultValue;
    const uint8_t mDefaultUnitType;
    const uint8_t mCtxType;
  };

  struct LengthAttributesInfo {
    SVGAnimatedLength* const mLengths;
    const LengthInfo* const mLengthInfo;
    const uint32_t mLengthCount;

    LengthAttributesInfo(SVGAnimatedLength* aLengths, LengthInfo* aLengthInfo,
                         uint32_t aLengthCount)
        : mLengths(aLengths),
          mLengthInfo(aLengthInfo),
          mLengthCount(aLengthCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct NumberInfo {
    nsStaticAtom* const mName;
    const float mDefaultValue;
    const bool mPercentagesAllowed;
  };

  struct NumberAttributesInfo {
    SVGAnimatedNumber* const mNumbers;
    const NumberInfo* const mNumberInfo;
    const uint32_t mNumberCount;

    NumberAttributesInfo(SVGAnimatedNumber* aNumbers, NumberInfo* aNumberInfo,
                         uint32_t aNumberCount)
        : mNumbers(aNumbers),
          mNumberInfo(aNumberInfo),
          mNumberCount(aNumberCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct NumberPairInfo {
    nsStaticAtom* const mName;
    const float mDefaultValue1;
    const float mDefaultValue2;
  };

  struct NumberPairAttributesInfo {
    SVGAnimatedNumberPair* const mNumberPairs;
    const NumberPairInfo* const mNumberPairInfo;
    const uint32_t mNumberPairCount;

    NumberPairAttributesInfo(SVGAnimatedNumberPair* aNumberPairs,
                             NumberPairInfo* aNumberPairInfo,
                             uint32_t aNumberPairCount)
        : mNumberPairs(aNumberPairs),
          mNumberPairInfo(aNumberPairInfo),
          mNumberPairCount(aNumberPairCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct IntegerInfo {
    nsStaticAtom* const mName;
    const int32_t mDefaultValue;
  };

  struct IntegerAttributesInfo {
    SVGAnimatedInteger* const mIntegers;
    const IntegerInfo* const mIntegerInfo;
    const uint32_t mIntegerCount;

    IntegerAttributesInfo(SVGAnimatedInteger* aIntegers,
                          IntegerInfo* aIntegerInfo, uint32_t aIntegerCount)
        : mIntegers(aIntegers),
          mIntegerInfo(aIntegerInfo),
          mIntegerCount(aIntegerCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct IntegerPairInfo {
    nsStaticAtom* const mName;
    const int32_t mDefaultValue1;
    const int32_t mDefaultValue2;
  };

  struct IntegerPairAttributesInfo {
    SVGAnimatedIntegerPair* const mIntegerPairs;
    const IntegerPairInfo* const mIntegerPairInfo;
    const uint32_t mIntegerPairCount;

    IntegerPairAttributesInfo(SVGAnimatedIntegerPair* aIntegerPairs,
                              IntegerPairInfo* aIntegerPairInfo,
                              uint32_t aIntegerPairCount)
        : mIntegerPairs(aIntegerPairs),
          mIntegerPairInfo(aIntegerPairInfo),
          mIntegerPairCount(aIntegerPairCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct BooleanInfo {
    nsStaticAtom* const mName;
    const bool mDefaultValue;
  };

  struct BooleanAttributesInfo {
    SVGAnimatedBoolean* const mBooleans;
    const BooleanInfo* const mBooleanInfo;
    const uint32_t mBooleanCount;

    BooleanAttributesInfo(SVGAnimatedBoolean* aBooleans,
                          BooleanInfo* aBooleanInfo, uint32_t aBooleanCount)
        : mBooleans(aBooleans),
          mBooleanInfo(aBooleanInfo),
          mBooleanCount(aBooleanCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  friend class mozilla::SVGAnimatedEnumeration;

  struct EnumInfo {
    nsStaticAtom* const mName;
    const SVGEnumMapping* const mMapping;
    const uint16_t mDefaultValue;
  };

  struct EnumAttributesInfo {
    SVGAnimatedEnumeration* const mEnums;
    const EnumInfo* const mEnumInfo;
    const uint32_t mEnumCount;

    EnumAttributesInfo(SVGAnimatedEnumeration* aEnums, EnumInfo* aEnumInfo,
                       uint32_t aEnumCount)
        : mEnums(aEnums), mEnumInfo(aEnumInfo), mEnumCount(aEnumCount) {}

    void Reset(uint8_t aAttrEnum);
    void SetUnknownValue(uint8_t aAttrEnum);
  };

  struct NumberListInfo {
    nsStaticAtom* const mName;
  };

  struct NumberListAttributesInfo {
    SVGAnimatedNumberList* const mNumberLists;
    const NumberListInfo* const mNumberListInfo;
    const uint32_t mNumberListCount;

    NumberListAttributesInfo(SVGAnimatedNumberList* aNumberLists,
                             NumberListInfo* aNumberListInfo,
                             uint32_t aNumberListCount)
        : mNumberLists(aNumberLists),
          mNumberListInfo(aNumberListInfo),
          mNumberListCount(aNumberListCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct LengthListInfo {
    nsStaticAtom* const mName;
    const uint8_t mAxis;
    /**
     * Flag to indicate whether appending zeros to the end of the list would
     * change the rendering of the SVG for the attribute in question. For x and
     * y on the <text> element this is true, but for dx and dy on <text> this
     * is false. This flag is fed down to SVGLengthListSMILType so it can
     * determine if it can sensibly animate from-to lists of different lengths,
     * which is desirable in the case of dx and dy.
     */
    const bool mCouldZeroPadList;
  };

  struct LengthListAttributesInfo {
    SVGAnimatedLengthList* const mLengthLists;
    const LengthListInfo* const mLengthListInfo;
    const uint32_t mLengthListCount;

    LengthListAttributesInfo(SVGAnimatedLengthList* aLengthLists,
                             LengthListInfo* aLengthListInfo,
                             uint32_t aLengthListCount)
        : mLengthLists(aLengthLists),
          mLengthListInfo(aLengthListInfo),
          mLengthListCount(aLengthListCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  struct StringInfo {
    nsStaticAtom* const mName;
    const int32_t mNamespaceID;
    const bool mIsAnimatable;
  };

  struct StringAttributesInfo {
    SVGAnimatedString* const mStrings;
    const StringInfo* const mStringInfo;
    const uint32_t mStringCount;

    StringAttributesInfo(SVGAnimatedString* aStrings, StringInfo* aStringInfo,
                         uint32_t aStringCount)
        : mStrings(aStrings),
          mStringInfo(aStringInfo),
          mStringCount(aStringCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  friend class mozilla::DOMSVGStringList;

  struct StringListInfo {
    nsStaticAtom* const mName;
  };

  struct StringListAttributesInfo {
    SVGStringList* const mStringLists;
    const StringListInfo* const mStringListInfo;
    const uint32_t mStringListCount;

    StringListAttributesInfo(SVGStringList* aStringLists,
                             StringListInfo* aStringListInfo,
                             uint32_t aStringListCount)
        : mStringLists(aStringLists),
          mStringListInfo(aStringListInfo),
          mStringListCount(aStringListCount) {}

    void Reset(uint8_t aAttrEnum);
  };

  virtual LengthAttributesInfo GetLengthInfo();
  virtual NumberAttributesInfo GetNumberInfo();
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual IntegerPairAttributesInfo GetIntegerPairInfo();
  virtual BooleanAttributesInfo GetBooleanInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  // We assume all orients, viewboxes and preserveAspectRatios are alike
  // so we don't need to wrap the class
  virtual SVGAnimatedOrient* GetAnimatedOrient();
  virtual SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio();
  virtual SVGAnimatedViewBox* GetAnimatedViewBox();
  virtual NumberListAttributesInfo GetNumberListInfo();
  virtual LengthListAttributesInfo GetLengthListInfo();
  virtual StringAttributesInfo GetStringInfo();
  virtual StringListAttributesInfo GetStringListInfo();

  static SVGEnumMapping sSVGUnitTypesMap[];

 private:
  void UnsetAttrInternal(int32_t aNameSpaceID, nsAtom* aName, bool aNotify);

  SVGAnimatedClass mClassAttribute;
  nsAutoPtr<nsAttrValue> mClassAnimAttr;
  RefPtr<mozilla::DeclarationBlock> mContentDeclarationBlock;
};

/**
 * A macro to implement the NS_NewSVGXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_SVG_ELEMENT(_elementName)                            \
  nsresult NS_NewSVG##_elementName##Element(                                \
      nsIContent** aResult,                                                 \
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {               \
    RefPtr<mozilla::dom::SVG##_elementName##Element> it =                   \
        new mozilla::dom::SVG##_elementName##Element(std::move(aNodeInfo)); \
                                                                            \
    nsresult rv = it->Init();                                               \
                                                                            \
    if (NS_FAILED(rv)) {                                                    \
      return rv;                                                            \
    }                                                                       \
                                                                            \
    it.forget(aResult);                                                     \
                                                                            \
    return rv;                                                              \
  }

#define NS_IMPL_NS_NEW_SVG_ELEMENT_CHECK_PARSER(_elementName)              \
  nsresult NS_NewSVG##_elementName##Element(                               \
      nsIContent** aResult,                                                \
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,                \
      mozilla::dom::FromParser aFromParser) {                              \
    RefPtr<mozilla::dom::SVG##_elementName##Element> it =                  \
        new mozilla::dom::SVG##_elementName##Element(std::move(aNodeInfo), \
                                                     aFromParser);         \
                                                                           \
    nsresult rv = it->Init();                                              \
                                                                           \
    if (NS_FAILED(rv)) {                                                   \
      return rv;                                                           \
    }                                                                      \
                                                                           \
    it.forget(aResult);                                                    \
                                                                           \
    return rv;                                                             \
  }

// No unlinking, we'd need to null out the value pointer (the object it
// points to is held by the element) and null-check it everywhere.
#define NS_SVG_VAL_IMPL_CYCLE_COLLECTION(_val, _element) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(_val)                   \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_val)          \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_element)          \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                  \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_0(_val)

#define NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(_val, _element) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(_val)                                 \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_val)                          \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER                  \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                  \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_val)                        \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_element)                        \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                \
  NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_val)                           \
    NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER                   \
  NS_IMPL_CYCLE_COLLECTION_TRACE_END

}  // namespace dom
}  // namespace mozilla

#endif  // __NS_SVGELEMENT_H__
