/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#ifndef nsComputedDOMStyle_h__
#define nsComputedDOMStyle_h__

#include "mozilla/ArenaRefPtr.h"
#include "mozilla/ArenaRefPtrInlines.h"
#include "mozilla/Attributes.h"
#include "mozilla/StyleComplexColor.h"
#include "mozilla/UniquePtr.h"
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
} // namespace dom
struct ComputedGridTrackInfo;
} // namespace mozilla

struct ComputedStyleMap;
struct nsCSSKTableEntry;
class nsIFrame;
class nsIPresShell;
class nsDOMCSSValueList;
struct nsMargin;
class nsROCSSPrimitiveValue;
class nsStyleCoord;
class nsStyleCorners;
struct nsStyleFilter;
class nsStyleGradient;
struct nsStyleImage;
class nsStyleSides;
struct nsTimingFunction;

class nsComputedDOMStyle final : public nsDOMCSSDeclaration
                               , public nsStubMutationObserver
{
private:
  // Convenience typedefs:
  typedef nsCSSKTableEntry KTableEntry;
  typedef mozilla::dom::CSSValue CSSValue;
  typedef mozilla::StyleGeometryBox StyleGeometryBox;

  already_AddRefed<CSSValue>
  GetPropertyCSSValueWithoutWarning(const nsAString& aProp,
                                    mozilla::ErrorResult& aRv);

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsComputedDOMStyle,
                                                                   nsICSSDeclaration)

  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER
  nsresult GetPropertyValue(const nsCSSPropertyID aPropID,
                            nsAString& aValue) override;
  nsresult SetPropertyValue(const nsCSSPropertyID aPropID,
                            const nsAString& aValue,
                            nsIPrincipal* aSubjectPrincipal) override;

  // Do NOT use this, it is deprecated, see bug 474655.
  already_AddRefed<CSSValue>
  GetPropertyCSSValue(const nsAString& aProp, mozilla::ErrorResult& aRv) final;
  using nsICSSDeclaration::GetPropertyCSSValue;

  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) final;

  enum StyleType {
    eDefaultOnly, // Only includes UA and user sheets
    eAll // Includes all stylesheets
  };

  nsComputedDOMStyle(mozilla::dom::Element* aElement,
                     const nsAString& aPseudoElt,
                     nsIPresShell* aPresShell,
                     StyleType aStyleType);

  virtual nsINode *GetParentObject() override
  {
    return mContent;
  }

  static already_AddRefed<mozilla::ComputedStyle>
  GetComputedStyle(mozilla::dom::Element* aElement, nsAtom* aPseudo,
                   StyleType aStyleType = eAll);

  static already_AddRefed<mozilla::ComputedStyle>
  GetComputedStyleNoFlush(mozilla::dom::Element* aElement,
                          nsAtom* aPseudo,
                          StyleType aStyleType = eAll)
  {
    return DoGetComputedStyleNoFlush(aElement,
                                     aPseudo,
                                     nsContentUtils::GetPresShellForContent(aElement),
                                     aStyleType);
  }

  static already_AddRefed<mozilla::ComputedStyle>
  GetUnanimatedComputedStyleNoFlush(mozilla::dom::Element* aElement,
                                    nsAtom* aPseudo);

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
  virtual mozilla::DeclarationBlock* GetCSSDeclaration(Operation) override;
  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock*) override;
  virtual nsIDocument* DocToUpdate() override;

  nsDOMCSSDeclaration::ServoCSSParsingEnvironment
  GetServoCSSParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final;

  static already_AddRefed<nsROCSSPrimitiveValue>
    MatrixToCSSValue(const mozilla::gfx::Matrix4x4& aMatrix);

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

  // Helper method for DoGetTextAlign[Last].
  already_AddRefed<CSSValue> CreateTextAlignValue(uint8_t aAlign,
                                                  bool aAlignTrue,
                                                  const KTableEntry aTable[]);
  // This indicates error by leaving mComputedStyle null.
  void UpdateCurrentStyleSources(bool aNeedsLayoutFlush);
  void ClearCurrentStyleSources();

  // Helper functions called by UpdateCurrentStyleSources.
  void ClearComputedStyle();
  void SetResolvedComputedStyle(RefPtr<mozilla::ComputedStyle>&& aContext,
                                uint64_t aGeneration);
  void SetFrameComputedStyle(mozilla::ComputedStyle* aStyle, uint64_t aGeneration);

  static already_AddRefed<mozilla::ComputedStyle>
  DoGetComputedStyleNoFlush(mozilla::dom::Element* aElement,
                            nsAtom* aPseudo,
                            nsIPresShell* aPresShell,
                            StyleType aStyleType);

#define STYLE_STRUCT(name_)                 \
  const nsStyle##name_ * Style##name_() {   \
    return mComputedStyle->Style##name_();  \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  already_AddRefed<CSSValue> GetEllipseRadii(const nsStyleCorners& aRadius,
                                             mozilla::Corner aFullCorner);

  already_AddRefed<CSSValue> GetOffsetWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetAbsoluteOffset(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetRelativeOffset(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetStickyOffset(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetStaticOffset(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetPaddingWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetBorderStyleFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetBorderWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetBorderColorFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetMarginWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetFallbackValue(const nsStyleSVGPaint* aPaint);

  already_AddRefed<CSSValue> GetSVGPaintFor(bool aFill);

  already_AddRefed<CSSValue> GetTransformValue(nsCSSValueSharedList* aSpecifiedTransform);

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

  already_AddRefed<CSSValue> GetCSSShadowArray(nsCSSShadowArray* aArray,
                                               const nscolor& aDefaultColor,
                                               bool aIsBoxShadow);

  void GetCSSGradientString(const nsStyleGradient* aGradient,
                            nsAString& aString);
  void GetImageRectString(nsIURI* aURI,
                          const nsStyleSides& aCropRect,
                          nsString& aString);
  already_AddRefed<CSSValue> GetScrollSnapPoints(const nsStyleCoord& aCoord);
  void AppendTimingFunction(nsDOMCSSValueList *aValueList,
                            const nsTimingFunction& aTimingFunction);

  bool ShouldHonorMinSizeAutoInAxis(mozilla::PhysicalAxis aAxis);

  /* Properties queryable as CSSValues.
   * To avoid a name conflict with nsIDOM*CSS2Properties, these are all
   * DoGetXXX instead of GetXXX.
   */

  already_AddRefed<CSSValue> DoGetAppearance();

  /* Box properties */
  already_AddRefed<CSSValue> DoGetBoxAlign();
  already_AddRefed<CSSValue> DoGetBoxDecorationBreak();
  already_AddRefed<CSSValue> DoGetBoxDirection();
  already_AddRefed<CSSValue> DoGetBoxFlex();
  already_AddRefed<CSSValue> DoGetBoxOrdinalGroup();
  already_AddRefed<CSSValue> DoGetBoxOrient();
  already_AddRefed<CSSValue> DoGetBoxPack();
  already_AddRefed<CSSValue> DoGetBoxSizing();

  already_AddRefed<CSSValue> DoGetWidth();
  already_AddRefed<CSSValue> DoGetHeight();
  already_AddRefed<CSSValue> DoGetMaxHeight();
  already_AddRefed<CSSValue> DoGetMaxWidth();
  already_AddRefed<CSSValue> DoGetMinHeight();
  already_AddRefed<CSSValue> DoGetMinWidth();
  already_AddRefed<CSSValue> DoGetMixBlendMode();
  already_AddRefed<CSSValue> DoGetIsolation();
  already_AddRefed<CSSValue> DoGetObjectFit();
  already_AddRefed<CSSValue> DoGetObjectPosition();
  already_AddRefed<CSSValue> DoGetLeft();
  already_AddRefed<CSSValue> DoGetTop();
  already_AddRefed<CSSValue> DoGetRight();
  already_AddRefed<CSSValue> DoGetBottom();
  already_AddRefed<CSSValue> DoGetStackSizing();

  /* Font properties */
  already_AddRefed<CSSValue> DoGetColor();
  already_AddRefed<CSSValue> DoGetFontFamily();
  already_AddRefed<CSSValue> DoGetFontFeatureSettings();
  already_AddRefed<CSSValue> DoGetFontVariationSettings();
  already_AddRefed<CSSValue> DoGetFontKerning();
  already_AddRefed<CSSValue> DoGetFontLanguageOverride();
  already_AddRefed<CSSValue> DoGetFontOpticalSizing();
  already_AddRefed<CSSValue> DoGetFontSize();
  already_AddRefed<CSSValue> DoGetFontSizeAdjust();
  already_AddRefed<CSSValue> DoGetOsxFontSmoothing();
  already_AddRefed<CSSValue> DoGetFontSmoothingBackgroundColor();
  already_AddRefed<CSSValue> DoGetFontStretch();
  already_AddRefed<CSSValue> DoGetFontStyle();
  already_AddRefed<CSSValue> DoGetFontSynthesis();
  already_AddRefed<CSSValue> DoGetFontVariant();
  already_AddRefed<CSSValue> DoGetFontVariantAlternates();
  already_AddRefed<CSSValue> DoGetFontVariantCaps();
  already_AddRefed<CSSValue> DoGetFontVariantEastAsian();
  already_AddRefed<CSSValue> DoGetFontVariantLigatures();
  already_AddRefed<CSSValue> DoGetFontVariantNumeric();
  already_AddRefed<CSSValue> DoGetFontVariantPosition();
  already_AddRefed<CSSValue> DoGetFontWeight();

  /* Grid properties */
  already_AddRefed<CSSValue> DoGetGridAutoFlow();
  already_AddRefed<CSSValue> DoGetGridAutoColumns();
  already_AddRefed<CSSValue> DoGetGridAutoRows();
  already_AddRefed<CSSValue> DoGetGridTemplateAreas();
  already_AddRefed<CSSValue> DoGetGridTemplateColumns();
  already_AddRefed<CSSValue> DoGetGridTemplateRows();
  already_AddRefed<CSSValue> DoGetGridColumnStart();
  already_AddRefed<CSSValue> DoGetGridColumnEnd();
  already_AddRefed<CSSValue> DoGetGridRowStart();
  already_AddRefed<CSSValue> DoGetGridRowEnd();

  /* StyleImageLayer properties */
  already_AddRefed<CSSValue> DoGetImageLayerImage(const nsStyleImageLayers& aLayers);
  already_AddRefed<CSSValue> DoGetImageLayerPosition(const nsStyleImageLayers& aLayers);
  already_AddRefed<CSSValue> DoGetImageLayerPositionX(const nsStyleImageLayers& aLayers);
  already_AddRefed<CSSValue> DoGetImageLayerPositionY(const nsStyleImageLayers& aLayers);
  already_AddRefed<CSSValue> DoGetImageLayerRepeat(const nsStyleImageLayers& aLayers);
  already_AddRefed<CSSValue> DoGetImageLayerSize(const nsStyleImageLayers& aLayers);

  /* Background properties */
  already_AddRefed<CSSValue> DoGetBackgroundAttachment();
  already_AddRefed<CSSValue> DoGetBackgroundColor();
  already_AddRefed<CSSValue> DoGetBackgroundImage();
  already_AddRefed<CSSValue> DoGetBackgroundPosition();
  already_AddRefed<CSSValue> DoGetBackgroundPositionX();
  already_AddRefed<CSSValue> DoGetBackgroundPositionY();
  already_AddRefed<CSSValue> DoGetBackgroundRepeat();
  already_AddRefed<CSSValue> DoGetBackgroundClip();
  already_AddRefed<CSSValue> DoGetBackgroundBlendMode();
  already_AddRefed<CSSValue> DoGetBackgroundOrigin();
  already_AddRefed<CSSValue> DoGetBackgroundSize();

  /* Mask properties */
  already_AddRefed<CSSValue> DoGetMask();
  already_AddRefed<CSSValue> DoGetMaskImage();
  already_AddRefed<CSSValue> DoGetMaskPosition();
  already_AddRefed<CSSValue> DoGetMaskPositionX();
  already_AddRefed<CSSValue> DoGetMaskPositionY();
  already_AddRefed<CSSValue> DoGetMaskRepeat();
  already_AddRefed<CSSValue> DoGetMaskClip();
  already_AddRefed<CSSValue> DoGetMaskOrigin();
  already_AddRefed<CSSValue> DoGetMaskSize();
  already_AddRefed<CSSValue> DoGetMaskMode();
  already_AddRefed<CSSValue> DoGetMaskComposite();

  /* Padding properties */
  already_AddRefed<CSSValue> DoGetPaddingTop();
  already_AddRefed<CSSValue> DoGetPaddingBottom();
  already_AddRefed<CSSValue> DoGetPaddingLeft();
  already_AddRefed<CSSValue> DoGetPaddingRight();

  /* Table Properties */
  already_AddRefed<CSSValue> DoGetBorderCollapse();
  already_AddRefed<CSSValue> DoGetBorderSpacing();
  already_AddRefed<CSSValue> DoGetCaptionSide();
  already_AddRefed<CSSValue> DoGetEmptyCells();
  already_AddRefed<CSSValue> DoGetTableLayout();
  already_AddRefed<CSSValue> DoGetVerticalAlign();

  /* Border Properties */
  already_AddRefed<CSSValue> DoGetBorderTopStyle();
  already_AddRefed<CSSValue> DoGetBorderBottomStyle();
  already_AddRefed<CSSValue> DoGetBorderLeftStyle();
  already_AddRefed<CSSValue> DoGetBorderRightStyle();
  already_AddRefed<CSSValue> DoGetBorderTopWidth();
  already_AddRefed<CSSValue> DoGetBorderBottomWidth();
  already_AddRefed<CSSValue> DoGetBorderLeftWidth();
  already_AddRefed<CSSValue> DoGetBorderRightWidth();
  already_AddRefed<CSSValue> DoGetBorderTopColor();
  already_AddRefed<CSSValue> DoGetBorderBottomColor();
  already_AddRefed<CSSValue> DoGetBorderLeftColor();
  already_AddRefed<CSSValue> DoGetBorderRightColor();
  already_AddRefed<CSSValue> DoGetBorderBottomLeftRadius();
  already_AddRefed<CSSValue> DoGetBorderBottomRightRadius();
  already_AddRefed<CSSValue> DoGetBorderTopLeftRadius();
  already_AddRefed<CSSValue> DoGetBorderTopRightRadius();
  already_AddRefed<CSSValue> DoGetFloatEdge();

  /* Border Image */
  already_AddRefed<CSSValue> DoGetBorderImageSource();
  already_AddRefed<CSSValue> DoGetBorderImageSlice();
  already_AddRefed<CSSValue> DoGetBorderImageWidth();
  already_AddRefed<CSSValue> DoGetBorderImageOutset();
  already_AddRefed<CSSValue> DoGetBorderImageRepeat();

  /* Box Shadow */
  already_AddRefed<CSSValue> DoGetBoxShadow();

  /* Window Shadow */
  already_AddRefed<CSSValue> DoGetWindowShadow();

  /* Margin Properties */
  already_AddRefed<CSSValue> DoGetMarginTopWidth();
  already_AddRefed<CSSValue> DoGetMarginBottomWidth();
  already_AddRefed<CSSValue> DoGetMarginLeftWidth();
  already_AddRefed<CSSValue> DoGetMarginRightWidth();

  /* Outline Properties */
  already_AddRefed<CSSValue> DoGetOutlineWidth();
  already_AddRefed<CSSValue> DoGetOutlineStyle();
  already_AddRefed<CSSValue> DoGetOutlineColor();
  already_AddRefed<CSSValue> DoGetOutlineOffset();
  already_AddRefed<CSSValue> DoGetOutlineRadiusBottomLeft();
  already_AddRefed<CSSValue> DoGetOutlineRadiusBottomRight();
  already_AddRefed<CSSValue> DoGetOutlineRadiusTopLeft();
  already_AddRefed<CSSValue> DoGetOutlineRadiusTopRight();

  /* Content Properties */
  already_AddRefed<CSSValue> DoGetContent();
  already_AddRefed<CSSValue> DoGetCounterIncrement();
  already_AddRefed<CSSValue> DoGetCounterReset();

  /* Quotes Properties */
  already_AddRefed<CSSValue> DoGetQuotes();

  /* z-index */
  already_AddRefed<CSSValue> DoGetZIndex();

  /* List properties */
  already_AddRefed<CSSValue> DoGetListStyleImage();
  already_AddRefed<CSSValue> DoGetListStylePosition();
  already_AddRefed<CSSValue> DoGetListStyleType();
  already_AddRefed<CSSValue> DoGetImageRegion();

  /* Text Properties */
  already_AddRefed<CSSValue> DoGetInitialLetter();
  already_AddRefed<CSSValue> DoGetLineHeight();
  already_AddRefed<CSSValue> DoGetRubyAlign();
  already_AddRefed<CSSValue> DoGetRubyPosition();
  already_AddRefed<CSSValue> DoGetTextAlign();
  already_AddRefed<CSSValue> DoGetTextAlignLast();
  already_AddRefed<CSSValue> DoGetTextCombineUpright();
  already_AddRefed<CSSValue> DoGetTextDecoration();
  already_AddRefed<CSSValue> DoGetTextDecorationColor();
  already_AddRefed<CSSValue> DoGetTextDecorationLine();
  already_AddRefed<CSSValue> DoGetTextDecorationStyle();
  already_AddRefed<CSSValue> DoGetTextEmphasisColor();
  already_AddRefed<CSSValue> DoGetTextEmphasisPosition();
  already_AddRefed<CSSValue> DoGetTextEmphasisStyle();
  already_AddRefed<CSSValue> DoGetTextIndent();
  already_AddRefed<CSSValue> DoGetTextJustify();
  already_AddRefed<CSSValue> DoGetTextOrientation();
  already_AddRefed<CSSValue> DoGetTextOverflow();
  already_AddRefed<CSSValue> DoGetTextShadow();
  already_AddRefed<CSSValue> DoGetTextSizeAdjust();
  already_AddRefed<CSSValue> DoGetTextTransform();
  already_AddRefed<CSSValue> DoGetLetterSpacing();
  already_AddRefed<CSSValue> DoGetWordSpacing();
  already_AddRefed<CSSValue> DoGetWhiteSpace();
  already_AddRefed<CSSValue> DoGetWordBreak();
  already_AddRefed<CSSValue> DoGetOverflowWrap();
  already_AddRefed<CSSValue> DoGetHyphens();
  already_AddRefed<CSSValue> DoGetTabSize();
  already_AddRefed<CSSValue> DoGetWebkitTextFillColor();
  already_AddRefed<CSSValue> DoGetWebkitTextStrokeColor();
  already_AddRefed<CSSValue> DoGetWebkitTextStrokeWidth();

  /* Visibility properties */
  already_AddRefed<CSSValue> DoGetColorAdjust();
  already_AddRefed<CSSValue> DoGetOpacity();
  already_AddRefed<CSSValue> DoGetPointerEvents();
  already_AddRefed<CSSValue> DoGetVisibility();
  already_AddRefed<CSSValue> DoGetWritingMode();

  /* Direction properties */
  already_AddRefed<CSSValue> DoGetDirection();
  already_AddRefed<CSSValue> DoGetUnicodeBidi();

  /* Display properties */
  already_AddRefed<CSSValue> DoGetBinding();
  already_AddRefed<CSSValue> DoGetClear();
  already_AddRefed<CSSValue> DoGetFloat();
  already_AddRefed<CSSValue> DoGetDisplay();
  already_AddRefed<CSSValue> DoGetContain();
  already_AddRefed<CSSValue> DoGetPosition();
  already_AddRefed<CSSValue> DoGetClip();
  already_AddRefed<CSSValue> DoGetImageOrientation();
  already_AddRefed<CSSValue> DoGetWillChange();
  already_AddRefed<CSSValue> DoGetOverflow();
  already_AddRefed<CSSValue> DoGetOverflowX();
  already_AddRefed<CSSValue> DoGetOverflowY();
  already_AddRefed<CSSValue> DoGetOverflowClipBoxBlock();
  already_AddRefed<CSSValue> DoGetOverflowClipBoxInline();
  already_AddRefed<CSSValue> DoGetResize();
  already_AddRefed<CSSValue> DoGetPageBreakAfter();
  already_AddRefed<CSSValue> DoGetPageBreakBefore();
  already_AddRefed<CSSValue> DoGetPageBreakInside();
  already_AddRefed<CSSValue> DoGetTouchAction();
  already_AddRefed<CSSValue> DoGetTransform();
  already_AddRefed<CSSValue> DoGetTranslate();
  already_AddRefed<CSSValue> DoGetRotate();
  already_AddRefed<CSSValue> DoGetScale();
  already_AddRefed<CSSValue> DoGetTransformBox();
  already_AddRefed<CSSValue> DoGetTransformOrigin();
  already_AddRefed<CSSValue> DoGetPerspective();
  already_AddRefed<CSSValue> DoGetBackfaceVisibility();
  already_AddRefed<CSSValue> DoGetPerspectiveOrigin();
  already_AddRefed<CSSValue> DoGetTransformStyle();
  already_AddRefed<CSSValue> DoGetOrient();
  already_AddRefed<CSSValue> DoGetScrollBehavior();
  already_AddRefed<CSSValue> DoGetOverscrollBehaviorX();
  already_AddRefed<CSSValue> DoGetOverscrollBehaviorY();
  already_AddRefed<CSSValue> DoGetScrollSnapType();
  already_AddRefed<CSSValue> DoGetScrollSnapTypeX();
  already_AddRefed<CSSValue> DoGetScrollSnapTypeY();
  already_AddRefed<CSSValue> DoGetScrollSnapPointsX();
  already_AddRefed<CSSValue> DoGetScrollSnapPointsY();
  already_AddRefed<CSSValue> DoGetScrollSnapDestination();
  already_AddRefed<CSSValue> DoGetScrollSnapCoordinate();
  already_AddRefed<CSSValue> DoGetShapeImageThreshold();
  already_AddRefed<CSSValue> DoGetShapeMargin();
  already_AddRefed<CSSValue> DoGetShapeOutside();

  /* User interface properties */
  already_AddRefed<CSSValue> DoGetCaretColor();
  already_AddRefed<CSSValue> DoGetCursor();
  already_AddRefed<CSSValue> DoGetForceBrokenImageIcon();
  already_AddRefed<CSSValue> DoGetIMEMode();
  already_AddRefed<CSSValue> DoGetUserFocus();
  already_AddRefed<CSSValue> DoGetUserInput();
  already_AddRefed<CSSValue> DoGetUserModify();
  already_AddRefed<CSSValue> DoGetUserSelect();
  already_AddRefed<CSSValue> DoGetWindowDragging();
  already_AddRefed<CSSValue> DoGetWindowOpacity();
  already_AddRefed<CSSValue> DoGetWindowTransform();
  already_AddRefed<CSSValue> DoGetWindowTransformOrigin();

  /* Column properties */
  already_AddRefed<CSSValue> DoGetColumnCount();
  already_AddRefed<CSSValue> DoGetColumnFill();
  already_AddRefed<CSSValue> DoGetColumnSpan();
  already_AddRefed<CSSValue> DoGetColumnWidth();
  already_AddRefed<CSSValue> DoGetColumnRuleWidth();
  already_AddRefed<CSSValue> DoGetColumnRuleStyle();
  already_AddRefed<CSSValue> DoGetColumnRuleColor();

  /* CSS Transitions */
  already_AddRefed<CSSValue> DoGetTransitionProperty();
  already_AddRefed<CSSValue> DoGetTransitionDuration();
  already_AddRefed<CSSValue> DoGetTransitionDelay();
  already_AddRefed<CSSValue> DoGetTransitionTimingFunction();

  /* CSS Animations */
  already_AddRefed<CSSValue> DoGetAnimationName();
  already_AddRefed<CSSValue> DoGetAnimationDuration();
  already_AddRefed<CSSValue> DoGetAnimationDelay();
  already_AddRefed<CSSValue> DoGetAnimationTimingFunction();
  already_AddRefed<CSSValue> DoGetAnimationDirection();
  already_AddRefed<CSSValue> DoGetAnimationFillMode();
  already_AddRefed<CSSValue> DoGetAnimationIterationCount();
  already_AddRefed<CSSValue> DoGetAnimationPlayState();

  /* CSS Flexbox properties */
  already_AddRefed<CSSValue> DoGetFlexBasis();
  already_AddRefed<CSSValue> DoGetFlexDirection();
  already_AddRefed<CSSValue> DoGetFlexGrow();
  already_AddRefed<CSSValue> DoGetFlexShrink();
  already_AddRefed<CSSValue> DoGetFlexWrap();

  /* CSS Flexbox/Grid properties */
  already_AddRefed<CSSValue> DoGetOrder();

  /* CSS Box Alignment properties */
  already_AddRefed<CSSValue> DoGetAlignContent();
  already_AddRefed<CSSValue> DoGetAlignItems();
  already_AddRefed<CSSValue> DoGetAlignSelf();
  already_AddRefed<CSSValue> DoGetJustifyContent();
  already_AddRefed<CSSValue> DoGetJustifyItems();
  already_AddRefed<CSSValue> DoGetJustifySelf();
  already_AddRefed<CSSValue> DoGetColumnGap();
  already_AddRefed<CSSValue> DoGetRowGap();

  /* SVG properties */
  already_AddRefed<CSSValue> DoGetFill();
  already_AddRefed<CSSValue> DoGetStroke();
  already_AddRefed<CSSValue> DoGetMarkerEnd();
  already_AddRefed<CSSValue> DoGetMarkerMid();
  already_AddRefed<CSSValue> DoGetMarkerStart();
  already_AddRefed<CSSValue> DoGetStrokeDasharray();

  already_AddRefed<CSSValue> DoGetStrokeDashoffset();
  already_AddRefed<CSSValue> DoGetStrokeWidth();
  already_AddRefed<CSSValue> DoGetVectorEffect();

  already_AddRefed<CSSValue> DoGetFillOpacity();
  already_AddRefed<CSSValue> DoGetFloodOpacity();
  already_AddRefed<CSSValue> DoGetStopOpacity();
  already_AddRefed<CSSValue> DoGetStrokeMiterlimit();
  already_AddRefed<CSSValue> DoGetStrokeOpacity();

  already_AddRefed<CSSValue> DoGetClipRule();
  already_AddRefed<CSSValue> DoGetFillRule();
  already_AddRefed<CSSValue> DoGetStrokeLinecap();
  already_AddRefed<CSSValue> DoGetStrokeLinejoin();
  already_AddRefed<CSSValue> DoGetTextAnchor();

  already_AddRefed<CSSValue> DoGetColorInterpolation();
  already_AddRefed<CSSValue> DoGetColorInterpolationFilters();
  already_AddRefed<CSSValue> DoGetDominantBaseline();
  already_AddRefed<CSSValue> DoGetImageRendering();
  already_AddRefed<CSSValue> DoGetShapeRendering();
  already_AddRefed<CSSValue> DoGetTextRendering();

  already_AddRefed<CSSValue> DoGetFloodColor();
  already_AddRefed<CSSValue> DoGetLightingColor();
  already_AddRefed<CSSValue> DoGetStopColor();

  already_AddRefed<CSSValue> DoGetClipPath();
  already_AddRefed<CSSValue> DoGetFilter();
  already_AddRefed<CSSValue> DoGetMaskType();
  already_AddRefed<CSSValue> DoGetPaintOrder();

  already_AddRefed<CSSValue> DoGetContextProperties();

  /* Custom properties */
  already_AddRefed<CSSValue> DoGetCustomProperty(const nsAString& aPropertyName);

  /* Helper functions */
  void SetToRGBAColor(nsROCSSPrimitiveValue* aValue, nscolor aColor);
  void SetValueFromComplexColor(nsROCSSPrimitiveValue* aValue,
                                const mozilla::StyleComplexColor& aColor);
  void SetValueToStyleImage(const nsStyleImage& aStyleImage,
                            nsROCSSPrimitiveValue* aValue);
  void SetValueToPositionCoord(const mozilla::Position::Coord& aCoord,
                               nsROCSSPrimitiveValue* aValue);
  void SetValueToPosition(const mozilla::Position& aPosition,
                          nsDOMCSSValueList* aValueList);
  void SetValueToURLValue(const mozilla::css::URLValueData* aURL,
                          nsROCSSPrimitiveValue* aValue);

  /**
   * A method to get a percentage base for a percentage value.  Returns true
   * if a percentage base value was determined, false otherwise.
   */
  typedef bool (nsComputedDOMStyle::*PercentageBaseGetter)(nscoord&);

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
                       const nsStyleCoord& aCoord,
                       bool aClampNegativeCalc,
                       PercentageBaseGetter aPercentageBaseGetter = nullptr,
                       const KTableEntry aTable[] = nullptr,
                       nscoord aMinAppUnits = nscoord_MIN,
                       nscoord aMaxAppUnits = nscoord_MAX);

  /**
   * If aCoord is a eStyleUnit_Coord returns the nscoord.  If it's
   * eStyleUnit_Percent, attempts to resolve the percentage base and returns
   * the resulting nscoord.  If it's some other unit or a percentge base can't
   * be determined, returns aDefaultValue.
   */
  nscoord StyleCoordToNSCoord(const nsStyleCoord& aCoord,
                              PercentageBaseGetter aPercentageBaseGetter,
                              nscoord aDefaultValue,
                              bool aClampNegativeCalc);

  /**
   * Append coord values from four sides. It omits values when possible.
   */
  void AppendFourSideCoordValues(nsDOMCSSValueList* aList,
                                 const nsStyleSides& aValues);

  bool GetCBContentWidth(nscoord& aWidth);
  bool GetCBContentHeight(nscoord& aWidth);
  bool GetScrollFrameContentWidth(nscoord& aWidth);
  bool GetScrollFrameContentHeight(nscoord& aHeight);
  bool GetFrameBoundsWidthForTransform(nscoord &aWidth);
  bool GetFrameBoundsHeightForTransform(nscoord &aHeight);
  bool GetFrameBorderRectWidth(nscoord& aWidth);
  bool GetFrameBorderRectHeight(nscoord& aHeight);

  /* Helper functions for computing and serializing a nsStyleCoord. */
  void SetCssTextToCoord(nsAString& aCssText, const nsStyleCoord& aCoord,
                         bool aClampNegativeCalc);
  already_AddRefed<CSSValue> CreatePrimitiveValueForStyleFilter(
    const nsStyleFilter& aStyleFilter);

  already_AddRefed<CSSValue>
  GetShapeSource(const mozilla::StyleShapeSource& aShapeSource,
                 const KTableEntry aBoxKeywordTable[]);

  template<typename ReferenceBox>
  already_AddRefed<CSSValue>
  CreatePrimitiveValueForShapeSource(
    const mozilla::UniquePtr<mozilla::StyleBasicShape>& aStyleBasicShape,
    ReferenceBox aReferenceBox,
    const KTableEntry aBoxKeywordTable[]);

  // Helper function for computing basic shape styles.
  already_AddRefed<CSSValue> CreatePrimitiveValueForBasicShape(
    const mozilla::UniquePtr<mozilla::StyleBasicShape>& aStyleBasicShape);
  void BoxValuesToString(nsAString& aString,
                         const nsTArray<nsStyleCoord>& aBoxValues,
                         bool aClampNegativeCalc);
  void BasicShapeRadiiToString(nsAString& aCssText,
                               const nsStyleCorners& aCorners);

  // Find out if we can safely skip flushing for aDocument (i.e. pending
  // restyles does not affect mContent).
  bool NeedsToFlush(nsIDocument* aDocument) const;


  static ComputedStyleMap* GetComputedStyleMap();

  // We don't really have a good immutable representation of "presentation".
  // Given the way GetComputedStyle is currently used, we should just grab the
  // 0th presshell, if any, from the document.
  nsWeakPtr mDocumentWeak;
  nsCOMPtr<nsIContent> mContent;

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
   *
   * Since an ArenaRefPtr is used to hold the ComputedStyle, it will be cleared
   * if the pres arena from which it was allocated goes away.
   */
  mozilla::ArenaRefPtr<mozilla::ComputedStyle> mComputedStyle;
  RefPtr<nsAtom> mPseudo;

  /*
   * While computing style data, the primary frame for mContent --- named "outer"
   * because we should use it to compute positioning data.  Null
   * otherwise.
   */
  nsIFrame* mOuterFrame;
  /*
   * While computing style data, the "inner frame" for mContent --- the frame
   * which we should use to compute margin, border, padding and content data.  Null
   * otherwise.
   */
  nsIFrame* mInnerFrame;
  /*
   * While computing style data, the presshell we're working with.  Null
   * otherwise.
   */
  nsIPresShell* mPresShell;

  /*
   * The kind of styles we should be returning.
   */
  StyleType mStyleType;

  /**
   * The nsComputedDOMStyle generation at the time we last resolved a style
   * context and stored it in mComputedStyle.
   */
  uint64_t mComputedStyleGeneration;

  bool mExposeVisitedStyle;

  /**
   * Whether we resolved a ComputedStyle last time we called
   * UpdateCurrentStyleSources.  Initially false.
   */
  bool mResolvedComputedStyle;

#ifdef DEBUG
  bool mFlushedPendingReflows;
#endif
};

already_AddRefed<nsComputedDOMStyle>
NS_NewComputedDOMStyle(mozilla::dom::Element* aElement,
                       const nsAString& aPseudoElt,
                       nsIPresShell* aPresShell,
                       nsComputedDOMStyle::StyleType aStyleType =
                         nsComputedDOMStyle::eAll);

#endif /* nsComputedDOMStyle_h__ */
