/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __NS_SVGELEMENT_H__
#define __NS_SVGELEMENT_H__

/*
  nsSVGElement is the base class for all SVG content elements.
  It implements all the common DOM interfaces and handles attributes.
*/

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIDOMSVGElement.h"
#include "nsGenericElement.h"
#include "nsStyledElement.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsICSSStyleRule.h"

#ifdef MOZ_SMIL
#include "nsISMILAttr.h"
#include "nsSMILAnimationController.h"
#endif

class nsSVGSVGElement;
class nsSVGLength2;
class nsSVGNumber2;
class nsSVGInteger;
class nsSVGAngle;
class nsSVGBoolean;
class nsSVGEnum;
struct nsSVGEnumMapping;
class nsSVGViewBox;
class nsSVGPreserveAspectRatio;
class nsSVGString;
struct gfxMatrix;
namespace mozilla {
class SVGAnimatedLengthList;
class SVGUserUnitList;
class SVGAnimatedPathSegList;
}

typedef nsStyledElement nsSVGElementBase;

class nsSVGElement : public nsSVGElementBase,    // nsIContent
                     public nsISVGValueObserver  // :nsISupportsWeakReference
{
protected:
  nsSVGElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();
  virtual ~nsSVGElement();

public:
  typedef mozilla::SVGUserUnitList SVGUserUnitList;
  typedef mozilla::SVGAnimatedLengthList SVGAnimatedLengthList;
  typedef mozilla::SVGAnimatedPathSegList SVGAnimatedPathSegList;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface methods

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);

  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;

  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;

  virtual already_AddRefed<nsIURI> GetBaseURI() const;

  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);

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
                         PRBool* aReturn);
  
  // nsIDOMSVGElement
  NS_IMETHOD GetId(nsAString & aId);
  NS_IMETHOD SetId(const nsAString & aId);
  NS_IMETHOD GetOwnerSVGElement(nsIDOMSVGSVGElement** aOwnerSVGElement);
  NS_IMETHOD GetViewportElement(nsIDOMSVGElement** aViewportElement);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

  // Gets the element that establishes the rectangular viewport against which
  // we should resolve percentage lengths (our "coordinate context"). Returns
  // nsnull for outer <svg> or SVG without an <svg> parent (invalid SVG).
  nsSVGSVGElement* GetCtx();

  /**
   * Returns aMatrix post-multiplied by the transform from the userspace
   * established by this element to the userspace established by its parent.
   */
  virtual gfxMatrix PrependLocalTransformTo(const gfxMatrix &aMatrix);

  // Setter for to set the current <animateMotion> transformation
  // Only visible for nsSVGGraphicElement, so it's a no-op here, and that
  // subclass has the useful implementation.
  virtual void SetAnimateMotionTransform(const gfxMatrix* aMatrix) {/*no-op*/}

  PRBool IsStringAnimatable(PRUint8 aAttrEnum) {
    return GetStringInfo().mStringInfo[aAttrEnum].mIsAnimatable;
  }
  virtual void DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeNumber(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeInteger(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeAngle(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeBoolean(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeEnum(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangeViewBox(PRBool aDoSetAttr);
  virtual void DidChangePreserveAspectRatio(PRBool aDoSetAttr);
  virtual void DidChangeLengthList(PRUint8 aAttrEnum, PRBool aDoSetAttr);
  virtual void DidChangePathSegList(PRBool aDoSetAttr);
  virtual void DidChangeString(PRUint8 aAttrEnum) {}

  virtual void DidAnimateLength(PRUint8 aAttrEnum);
  virtual void DidAnimateNumber(PRUint8 aAttrEnum);
  virtual void DidAnimateInteger(PRUint8 aAttrEnum);
  virtual void DidAnimateAngle(PRUint8 aAttrEnum);
  virtual void DidAnimateBoolean(PRUint8 aAttrEnum);
  virtual void DidAnimateEnum(PRUint8 aAttrEnum);
  virtual void DidAnimateViewBox();
  virtual void DidAnimatePreserveAspectRatio();
  virtual void DidAnimateLengthList(PRUint8 aAttrEnum);
  virtual void DidAnimatePathSegList();
  virtual void DidAnimateTransform();
  virtual void DidAnimateString(PRUint8 aAttrEnum);

  void GetAnimatedLengthValues(float *aFirst, ...);
  void GetAnimatedNumberValues(float *aFirst, ...);
  void GetAnimatedIntegerValues(PRInt32 *aFirst, ...);
  void GetAnimatedLengthListValues(SVGUserUnitList *aFirst, ...);
  SVGAnimatedLengthList* GetAnimatedLengthList(PRUint8 aAttrEnum);
  virtual SVGAnimatedPathSegList* GetAnimPathSegList() {
    // DOM interface 'SVGAnimatedPathData' (*inherited* by nsSVGPathElement)
    // has a member called 'animatedPathSegList' member, so we have a shorter
    // name so we don't get hidden by the GetAnimatedPathSegList declared by
    // NS_DECL_NSIDOMSVGANIMATEDPATHDATA.
    return nsnull;
  }

#ifdef MOZ_SMIL
  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 aNamespaceID, nsIAtom* aName);
  void AnimationNeedsResample();
  void FlushAnimations();
#else
  void AnimationNeedsResample() { /* do nothing */ }
  void FlushAnimations() { /* do nothing */ }
#endif

  virtual void RecompileScriptEventListeners();

  void GetStringBaseValue(PRUint8 aAttrEnum, nsAString& aResult) const;
  void SetStringBaseValue(PRUint8 aAttrEnum, const nsAString& aValue);

  virtual nsIAtom* GetPathDataAttrName() const {
    return nsnull;
  }

protected:
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAString* aValue, PRBool aNotify);
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult);
  static nsresult ReportAttributeParseFailure(nsIDocument* aDocument,
                                              nsIAtom* aAttribute,
                                              const nsAString& aValue);

  // Hooks for subclasses
  virtual PRBool IsEventName(nsIAtom* aName);

  void UpdateContentStyleRule();
#ifdef MOZ_SMIL
  void UpdateAnimatedContentStyleRule();
  nsICSSStyleRule* GetAnimatedContentStyleRule();
#endif // MOZ_SMIL

  nsISVGValue* GetMappedAttribute(PRInt32 aNamespaceID, nsIAtom* aName);
  nsresult AddMappedSVGValue(nsIAtom* aName, nsISupports* aValue,
                             PRInt32 aNamespaceID = kNameSpaceID_None);
  
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
    PRPackedBool mDefaultValue;
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
    PRPackedBool mCouldZeroPadList;
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
    PRPackedBool mIsAnimatable;
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

  virtual LengthAttributesInfo GetLengthInfo();
  virtual NumberAttributesInfo GetNumberInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual AngleAttributesInfo GetAngleInfo();
  virtual BooleanAttributesInfo GetBooleanInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  // We assume all viewboxes and preserveAspectRatios are alike
  // so we don't need to wrap the class
  virtual nsSVGViewBox *GetViewBox();
  virtual nsSVGPreserveAspectRatio *GetPreserveAspectRatio();
  virtual LengthListAttributesInfo GetLengthListInfo();
  virtual StringAttributesInfo GetStringInfo();

  static nsSVGEnumMapping sSVGUnitTypesMap[];

private:
  /* read <number-optional-number> */
  nsresult
  ParseNumberOptionalNumber(const nsAString& aValue,
                            PRUint32 aIndex1, PRUint32 aIndex2);

  /* read <integer-optional-integer> */
  nsresult
  ParseIntegerOptionalInteger(const nsAString& aValue,
                              PRUint32 aIndex1, PRUint32 aIndex2);

  void ResetOldStyleBaseType(nsISVGValue *svg_value);

  struct ObservableModificationData {
    // Only to be used if |name| is non-null.  Otherwise, modType will
    // be 0 to indicate NS_OK should be returned and 1 to indicate
    // NS_ERROR_UNEXPECTED should be returned.
    ObservableModificationData(const nsAttrName* aName, PRUint32 aModType):
      name(aName), modType(aModType)
    {}
    const nsAttrName* name;
    PRUint8 modType;
  };
  ObservableModificationData
    GetModificationDataForObservable(nsISVGValue* aObservable,
                                     nsISVGValue::modificationType aModType);

  nsCOMPtr<nsICSSStyleRule> mContentStyleRule;
  nsAttrAndChildArray mMappedAttributes;

  PRPackedBool mSuppressNotification;
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
