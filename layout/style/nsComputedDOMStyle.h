/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#ifndef nsComputedDOMStyle_h__
#define nsComputedDOMStyle_h__

#include "mozilla/Attributes.h"
#include "mozilla/StyleColorInlines.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Element.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nscore.h"
#include "nsDOMCSSDeclaration.h"
#include "mozilla/ComputedStyle.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/gfx/Types.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsStyleStruct.h"
#include "mozilla/WritingModes.h"

namespace mozilla {
namespace dom {
class DocGroup;
class Element;
}  // namespace dom
class PresShell;
struct ComputedGridTrackInfo;
}  // namespace mozilla

struct ComputedStyleMap;
struct nsCSSKTableEntry;
class nsIFrame;
class nsDOMCSSValueList;
struct nsMargin;
class nsROCSSPrimitiveValue;
class nsStyleCoord;
struct nsStyleFilter;
class nsStyleGradient;
struct nsStyleImage;
class nsStyleSides;

class nsComputedDOMStyle final : public nsDOMCSSDeclaration,
                                 public nsStubMutationObserver {
 private:
  // Convenience typedefs:
  using KTableEntry = nsCSSKTableEntry;
  using CSSValue = mozilla::dom::CSSValue;
  using StyleGeometryBox = mozilla::StyleGeometryBox;
  using Element = mozilla::dom::Element;
  using Document = mozilla::dom::Document;
  using StyleFlexBasis = mozilla::StyleFlexBasis;
  using StyleSize = mozilla::StyleSize;
  using StyleMaxSize = mozilla::StyleMaxSize;
  using LengthPercentage = mozilla::LengthPercentage;
  using LengthPercentageOrAuto = mozilla::LengthPercentageOrAuto;
  using StyleExtremumLength = mozilla::StyleExtremumLength;
  using ComputedStyle = mozilla::ComputedStyle;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(
      nsComputedDOMStyle, nsICSSDeclaration)

  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER
  nsresult GetPropertyValue(const nsCSSPropertyID aPropID,
                            nsAString& aValue) override;
  nsresult SetPropertyValue(const nsCSSPropertyID aPropID,
                            const nsAString& aValue,
                            nsIPrincipal* aSubjectPrincipal) override;

  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) final;

  enum StyleType {
    eDefaultOnly,  // Only includes UA and user sheets
    eAll           // Includes all stylesheets
  };

  nsComputedDOMStyle(Element* aElement, const nsAString& aPseudoElt,
                     Document* aDocument, StyleType aStyleType);

  nsINode* GetParentObject() override { return mElement; }

  static already_AddRefed<ComputedStyle> GetComputedStyle(
      Element* aElement, nsAtom* aPseudo, StyleType aStyleType = eAll);

  static already_AddRefed<ComputedStyle> GetComputedStyleNoFlush(
      Element* aElement, nsAtom* aPseudo, StyleType aStyleType = eAll) {
    return DoGetComputedStyleNoFlush(
        aElement, aPseudo, nsContentUtils::GetPresShellForContent(aElement),
        aStyleType);
  }

  static already_AddRefed<ComputedStyle> GetUnanimatedComputedStyleNoFlush(
      Element* aElement, nsAtom* aPseudo);

  // Helper for nsDOMWindowUtils::GetVisitedDependentComputedStyle
  void SetExposeVisitedStyle(bool aExpose) {
    NS_ASSERTION(aExpose != mExposeVisitedStyle, "should always be changing");
    mExposeVisitedStyle = aExpose;
  }

  void GetCSSImageURLs(const nsAString& aPropertyName,
                       nsTArray<nsString>& aImageURLs,
                       mozilla::ErrorResult& aRv) final;

  // nsDOMCSSDeclaration abstract methods which should never be called
  // on a nsComputedDOMStyle object, but must be defined to avoid
  // compile errors.
  mozilla::DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, mozilla::DeclarationBlock** aCreated) final;
  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock*,
                                     mozilla::MutationClosureData*) override;
  virtual mozilla::dom::Document* DocToUpdate() override;

  nsDOMCSSDeclaration::ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final;

  static already_AddRefed<nsROCSSPrimitiveValue> MatrixToCSSValue(
      const mozilla::gfx::Matrix4x4& aMatrix);
  static void SetToRGBAColor(nsROCSSPrimitiveValue* aValue, nscolor aColor);

  static void RegisterPrefChangeCallbacks();
  static void UnregisterPrefChangeCallbacks();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

 private:
  virtual ~nsComputedDOMStyle();

  void AssertFlushedPendingReflows() {
    NS_ASSERTION(mFlushedPendingReflows,
                 "property getter should have been marked layout-dependent");
  }

  nsMargin GetAdjustedValuesForBoxSizing();

  // This indicates error by leaving mComputedStyle null.
  void UpdateCurrentStyleSources(bool aNeedsLayoutFlush);
  void ClearCurrentStyleSources();

  // Helper functions called by UpdateCurrentStyleSources.
  void ClearComputedStyle();
  void SetResolvedComputedStyle(RefPtr<ComputedStyle>&& aContext,
                                uint64_t aGeneration);
  void SetFrameComputedStyle(ComputedStyle* aStyle, uint64_t aGeneration);

  static already_AddRefed<ComputedStyle> DoGetComputedStyleNoFlush(
      Element* aElement, nsAtom* aPseudo, mozilla::PresShell* aPresShell,
      StyleType aStyleType);

#define STYLE_STRUCT(name_)                \
  const nsStyle##name_* Style##name_() {   \
    return mComputedStyle->Style##name_(); \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  /**
   * A method to get a percentage base for a percentage value.  Returns true
   * if a percentage base value was determined, false otherwise.
   */
  typedef bool (nsComputedDOMStyle::*PercentageBaseGetter)(nscoord&);

  already_AddRefed<CSSValue> GetOffsetWidthFor(mozilla::Side);
  already_AddRefed<CSSValue> GetAbsoluteOffset(mozilla::Side);
  nscoord GetUsedAbsoluteOffset(mozilla::Side);
  already_AddRefed<CSSValue> GetNonStaticPositionOffset(
      mozilla::Side aSide, bool aResolveAuto, PercentageBaseGetter aWidthGetter,
      PercentageBaseGetter aHeightGetter);

  already_AddRefed<CSSValue> GetStaticOffset(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetPaddingWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetBorderStyleFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetBorderWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetBorderColorFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetMarginWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetTransformValue(const mozilla::StyleTransform&);

  // Appends all aLineNames (may be empty) space-separated to aResult.
  void AppendGridLineNames(nsString& aResult,
                           const nsTArray<nsString>& aLineNames);
  // Appends aLineNames as a CSSValue* to aValueList.  If aLineNames is empty
  // a value ("[]") is only appended if aSuppressEmptyList is false.
  void AppendGridLineNames(nsDOMCSSValueList* aValueList,
                           const nsTArray<nsString>& aLineNames,
                           bool aSuppressEmptyList = true);
  // Appends aLineNames1/2 (if non-empty) as a CSSValue* to aValueList.
  void AppendGridLineNames(nsDOMCSSValueList* aValueList,
                           const nsTArray<nsString>& aLineNames1,
                           const nsTArray<nsString>& aLineNames2);
  already_AddRefed<CSSValue> GetGridTrackSize(const nsStyleCoord& aMinSize,
                                              const nsStyleCoord& aMaxSize);
  already_AddRefed<CSSValue> GetGridTemplateColumnsRows(
      const nsStyleGridTemplate& aTrackList,
      const mozilla::ComputedGridTrackInfo* aTrackInfo);
  already_AddRefed<CSSValue> GetGridLine(const nsStyleGridLine& aGridLine);

  bool GetLineHeightCoord(nscoord& aCoord);

  bool ShouldHonorMinSizeAutoInAxis(mozilla::PhysicalAxis aAxis);

  /* Properties queryable as CSSValues.
   * To avoid a name conflict with nsIDOM*CSS2Properties, these are all
   * DoGetXXX instead of GetXXX.
   */

  /* Box properties */

  already_AddRefed<CSSValue> DoGetWidth();
  already_AddRefed<CSSValue> DoGetHeight();
  already_AddRefed<CSSValue> DoGetMaxHeight();
  already_AddRefed<CSSValue> DoGetMaxWidth();
  already_AddRefed<CSSValue> DoGetMinHeight();
  already_AddRefed<CSSValue> DoGetMinWidth();
  already_AddRefed<CSSValue> DoGetLeft();
  already_AddRefed<CSSValue> DoGetTop();
  already_AddRefed<CSSValue> DoGetRight();
  already_AddRefed<CSSValue> DoGetBottom();

  /* Font properties */
  already_AddRefed<CSSValue> DoGetOsxFontSmoothing();

  /* Grid properties */
  already_AddRefed<CSSValue> DoGetGridAutoColumns();
  already_AddRefed<CSSValue> DoGetGridAutoRows();
  already_AddRefed<CSSValue> DoGetGridTemplateColumns();
  already_AddRefed<CSSValue> DoGetGridTemplateRows();

  /* StyleImageLayer properties */
  already_AddRefed<CSSValue> DoGetImageLayerPosition(
      const nsStyleImageLayers& aLayers);

  /* Mask properties */
  already_AddRefed<CSSValue> DoGetMask();

  /* Padding properties */
  already_AddRefed<CSSValue> DoGetPaddingTop();
  already_AddRefed<CSSValue> DoGetPaddingBottom();
  already_AddRefed<CSSValue> DoGetPaddingLeft();
  already_AddRefed<CSSValue> DoGetPaddingRight();

  /* Table Properties */
  already_AddRefed<CSSValue> DoGetBorderSpacing();

  /* Border Properties */
  already_AddRefed<CSSValue> DoGetBorderTopWidth();
  already_AddRefed<CSSValue> DoGetBorderBottomWidth();
  already_AddRefed<CSSValue> DoGetBorderLeftWidth();
  already_AddRefed<CSSValue> DoGetBorderRightWidth();

  /* Margin Properties */
  already_AddRefed<CSSValue> DoGetMarginTopWidth();
  already_AddRefed<CSSValue> DoGetMarginBottomWidth();
  already_AddRefed<CSSValue> DoGetMarginLeftWidth();
  already_AddRefed<CSSValue> DoGetMarginRightWidth();

  /* Text Properties */
  already_AddRefed<CSSValue> DoGetLineHeight();
  already_AddRefed<CSSValue> DoGetTextDecoration();
  already_AddRefed<CSSValue> DoGetTextDecorationColor();
  already_AddRefed<CSSValue> DoGetTextDecorationStyle();

  /* Display properties */
  already_AddRefed<CSSValue> DoGetTransform();
  already_AddRefed<CSSValue> DoGetTransformOrigin();
  already_AddRefed<CSSValue> DoGetPerspectiveOrigin();

  /* Column properties */
  already_AddRefed<CSSValue> DoGetColumnRuleWidth();

  // For working around a MSVC bug. See related comment in
  // GenerateComputedDOMStyleGenerated.py.
  already_AddRefed<CSSValue> DummyGetter();

  /* Helper functions */
  void SetValueFromComplexColor(nsROCSSPrimitiveValue* aValue,
                                const mozilla::StyleColor& aColor);
  void SetValueToPosition(const mozilla::Position& aPosition,
                          nsDOMCSSValueList* aValueList);
  void SetValueToURLValue(const mozilla::css::URLValue* aURL,
                          nsROCSSPrimitiveValue* aValue);

  void SetValueToSize(nsROCSSPrimitiveValue* aValue, const mozilla::StyleSize&);

  void SetValueToLengthPercentageOrAuto(nsROCSSPrimitiveValue* aValue,
                                        const LengthPercentageOrAuto&,
                                        bool aClampNegativeCalc);

  void SetValueToLengthPercentage(nsROCSSPrimitiveValue* aValue,
                                  const LengthPercentage&,
                                  bool aClampNegativeCalc);

  void SetValueToMaxSize(nsROCSSPrimitiveValue* aValue, const StyleMaxSize&);

  void SetValueToExtremumLength(nsROCSSPrimitiveValue* aValue,
                                StyleExtremumLength);

  /**
   * Method to set aValue to aCoord.  If aCoord is a percentage value and
   * aPercentageBaseGetter is not null, aPercentageBaseGetter is called.  If it
   * returns true, the percentage base it outputs in its out param is used
   * to compute an nscoord value.  If the getter is null or returns false,
   * the percent value of aCoord is set as a percent value on aValue.  aTable,
   * if not null, is the keyword table to handle eStyleUnit_Enumerated.  When
   * calling SetAppUnits on aValue (for coord or percent values), the value
   * passed in will be clamped to be no less than aMinAppUnits and no more than
   * aMaxAppUnits.
   *
   * XXXbz should caller pass in some sort of bitfield indicating which units
   * can be expected or something?
   */
  void SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                       const nsStyleCoord& aCoord, bool aClampNegativeCalc,
                       PercentageBaseGetter aPercentageBaseGetter = nullptr,
                       const KTableEntry aTable[] = nullptr);

  /**
   * If aCoord is a eStyleUnit_Coord returns the nscoord.  If it's
   * eStyleUnit_Percent, attempts to resolve the percentage base and returns
   * the resulting nscoord.  If it's some other unit or a percentage base can't
   * be determined, returns aDefaultValue.
   */
  nscoord StyleCoordToNSCoord(const LengthPercentage& aCoord,
                              PercentageBaseGetter aPercentageBaseGetter,
                              nscoord aDefaultValue, bool aClampNegativeCalc);
  template <typename LengthPercentageLike>
  nscoord StyleCoordToNSCoord(const LengthPercentageLike& aCoord,
                              PercentageBaseGetter aPercentageBaseGetter,
                              nscoord aDefaultValue, bool aClampNegativeCalc) {
    if (aCoord.IsLengthPercentage()) {
      return StyleCoordToNSCoord(aCoord.AsLengthPercentage(),
                                 aPercentageBaseGetter, aDefaultValue,
                                 aClampNegativeCalc);
    }
    return aDefaultValue;
  }

  /**
   * Append coord values from four sides. It omits values when possible.
   */
  void AppendFourSideCoordValues(nsDOMCSSValueList* aList,
                                 const nsStyleSides& aValues);

  bool GetCBContentWidth(nscoord& aWidth);
  bool GetCBContentHeight(nscoord& aHeight);
  bool GetCBPaddingRectWidth(nscoord& aWidth);
  bool GetCBPaddingRectHeight(nscoord& aHeight);
  bool GetScrollFrameContentWidth(nscoord& aWidth);
  bool GetScrollFrameContentHeight(nscoord& aHeight);
  bool GetFrameBorderRectWidth(nscoord& aWidth);
  bool GetFrameBorderRectHeight(nscoord& aHeight);

  /* Helper functions for computing and serializing a nsStyleCoord. */
  void SetCssTextToCoord(nsAString& aCssText, const nsStyleCoord& aCoord,
                         bool aClampNegativeCalc);

  // Find out if we can safely skip flushing (i.e. pending restyles do not
  // affect mElement).
  bool NeedsToFlush() const;

  static ComputedStyleMap* GetComputedStyleMap();

  // We don't really have a good immutable representation of "presentation".
  // Given the way GetComputedStyle is currently used, we should just grab the
  // presshell, if any, from the document.
  nsWeakPtr mDocumentWeak;
  RefPtr<Element> mElement;

  /**
   * Strong reference to the ComputedStyle we access data from.  This can be
   * either a ComputedStyle we resolved ourselves or a ComputedStyle we got
   * from our frame.
   *
   * If we got the ComputedStyle from the frame, we clear out mComputedStyle
   * in ClearCurrentStyleSources.  If we resolved one ourselves, then
   * ClearCurrentStyleSources leaves it in mComputedStyle for use the next
   * time this nsComputedDOMStyle object is queried.  UpdateCurrentStyleSources
   * in this case will check that the ComputedStyle is still valid to be used,
   * by checking whether flush styles results in any restyles having been
   * processed.
   */
  RefPtr<ComputedStyle> mComputedStyle;
  RefPtr<nsAtom> mPseudo;

  /*
   * While computing style data, the primary frame for mContent --- named
   * "outer" because we should use it to compute positioning data.  Null
   * otherwise.
   */
  nsIFrame* mOuterFrame;
  /*
   * While computing style data, the "inner frame" for mContent --- the frame
   * which we should use to compute margin, border, padding and content data.
   * Null otherwise.
   */
  nsIFrame* mInnerFrame;
  /*
   * While computing style data, the presshell we're working with.  Null
   * otherwise.
   */
  mozilla::PresShell* mPresShell;

  /*
   * The kind of styles we should be returning.
   */
  StyleType mStyleType;

  /**
   * The nsComputedDOMStyle generation at the time we last resolved a style
   * context and stored it in mComputedStyle, and the pres shell we got the
   * style from. Should only be used together.
   */
  uint64_t mComputedStyleGeneration = 0;

  uint32_t mPresShellId = 0;

  bool mExposeVisitedStyle;

  /**
   * Whether we resolved a ComputedStyle last time we called
   * UpdateCurrentStyleSources.  Initially false.
   */
  bool mResolvedComputedStyle;

#ifdef DEBUG
  bool mFlushedPendingReflows;
#endif

  friend struct ComputedStyleMap;
};

already_AddRefed<nsComputedDOMStyle> NS_NewComputedDOMStyle(
    mozilla::dom::Element* aElement, const nsAString& aPseudoElt,
    mozilla::dom::Document* aDocument,
    nsComputedDOMStyle::StyleType aStyleType = nsComputedDOMStyle::eAll);

#endif /* nsComputedDOMStyle_h__ */
