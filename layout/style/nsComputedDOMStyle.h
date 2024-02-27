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
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nscore.h"
#include "nsDOMCSSDeclaration.h"
#include "mozilla/ComputedStyle.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/gfx/Types.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsStubMutationObserver.h"
#include "nsStyleStruct.h"
#include "mozilla/WritingModes.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "mozilla/dom/Element.h"

namespace mozilla {
enum class FlushType : uint8_t;
enum class PseudoStyleType : uint8_t;

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
class nsStyleGradient;

class nsComputedDOMStyle final : public nsDOMCSSDeclaration,
                                 public nsStubMutationObserver {
 private:
  // Convenience typedefs:
  template <typename T>
  using Span = mozilla::Span<T>;
  using KTableEntry = nsCSSKTableEntry;
  using CSSValue = mozilla::dom::CSSValue;
  using StyleGeometryBox = mozilla::StyleGeometryBox;
  using Element = mozilla::dom::Element;
  using Document = mozilla::dom::Document;
  using PseudoStyleType = mozilla::PseudoStyleType;
  using LengthPercentage = mozilla::LengthPercentage;
  using LengthPercentageOrAuto = mozilla::LengthPercentageOrAuto;
  using ComputedStyle = mozilla::ComputedStyle;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS_AMBIGUOUS(
      nsComputedDOMStyle, nsICSSDeclaration)

  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER

  void GetPropertyValue(const nsCSSPropertyID aPropID,
                        nsACString& aValue) override;
  void SetPropertyValue(const nsCSSPropertyID aPropID, const nsACString& aValue,
                        nsIPrincipal* aSubjectPrincipal,
                        mozilla::ErrorResult& aRv) override;

  void IndexedGetter(uint32_t aIndex, bool& aFound,
                     nsACString& aPropName) final;

  enum class StyleType : uint8_t {
    DefaultOnly,  // Only includes UA and user sheets
    All           // Includes all stylesheets
  };

  // In some cases, for legacy reasons, we forcefully return an empty style.
  enum class AlwaysReturnEmptyStyle : bool { No, Yes };

  nsComputedDOMStyle(Element*, PseudoStyleType,
                     nsAtom* aFunctionalPseudoParameter, Document*, StyleType,
                     AlwaysReturnEmptyStyle = AlwaysReturnEmptyStyle::No);

  nsINode* GetAssociatedNode() const override { return mElement; }
  nsINode* GetParentObject() const override { return mElement; }

  static already_AddRefed<const ComputedStyle> GetComputedStyle(
      Element* aElement, PseudoStyleType = PseudoStyleType::NotPseudo,
      nsAtom* aFunctionalPseudoParameter = nullptr, StyleType = StyleType::All);

  static already_AddRefed<const ComputedStyle> GetComputedStyleNoFlush(
      const Element* aElement,
      PseudoStyleType aPseudo = PseudoStyleType::NotPseudo,
      nsAtom* aFunctionalPseudoParameter = nullptr,
      StyleType aStyleType = StyleType::All) {
    return DoGetComputedStyleNoFlush(
        aElement, aPseudo, aFunctionalPseudoParameter,
        nsContentUtils::GetPresShellForContent(aElement), aStyleType);
  }

  static already_AddRefed<const ComputedStyle>
  GetUnanimatedComputedStyleNoFlush(
      Element*, PseudoStyleType = PseudoStyleType::NotPseudo,
      nsAtom* aFunctionalPseudoParameter = nullptr);

  // Helper for nsDOMWindowUtils::GetVisitedDependentComputedStyle
  void SetExposeVisitedStyle(bool aExpose) {
    NS_ASSERTION(aExpose != mExposeVisitedStyle, "should always be changing");
    mExposeVisitedStyle = aExpose;
  }

  void GetCSSImageURLs(const nsACString& aPropertyName,
                       nsTArray<nsCString>& aImageURLs,
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

  static void RegisterPrefChangeCallbacks();
  static void UnregisterPrefChangeCallbacks();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

 private:
  void GetPropertyValue(const nsCSSPropertyID aPropID,
                        const nsACString& aMaybeCustomPropertyNme,
                        nsACString& aValue);
  using nsDOMCSSDeclaration::GetPropertyValue;

  virtual ~nsComputedDOMStyle();

  void AssertFlushedPendingReflows() {
    NS_ASSERTION(mFlushedPendingReflows,
                 "property getter should have been marked layout-dependent");
  }

  nsMargin GetAdjustedValuesForBoxSizing();

  // This indicates error by leaving mComputedStyle null.
  void UpdateCurrentStyleSources(nsCSSPropertyID);
  void ClearCurrentStyleSources();

  // Helper functions called by UpdateCurrentStyleSources.
  void ClearComputedStyle();
  void SetResolvedComputedStyle(RefPtr<const ComputedStyle>&& aContext,
                                uint64_t aGeneration);
  void SetFrameComputedStyle(ComputedStyle* aStyle, uint64_t aGeneration);

  static already_AddRefed<const ComputedStyle> DoGetComputedStyleNoFlush(
      const Element*, PseudoStyleType, nsAtom* aFunctionalPseudoParameter,
      mozilla::PresShell*, StyleType);

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

  already_AddRefed<CSSValue> GetBorderWidthFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetMarginFor(mozilla::Side aSide);

  already_AddRefed<CSSValue> GetTransformValue(const mozilla::StyleTransform&);

  already_AddRefed<nsROCSSPrimitiveValue> GetGridTrackSize(
      const mozilla::StyleTrackSize&);
  already_AddRefed<nsROCSSPrimitiveValue> GetGridTrackBreadth(
      const mozilla::StyleTrackBreadth&);
  void SetValueToTrackBreadth(nsROCSSPrimitiveValue*,
                              const mozilla::StyleTrackBreadth&);
  already_AddRefed<CSSValue> GetGridTemplateColumnsRows(
      const mozilla::StyleGridTemplateComponent& aTrackList,
      const mozilla::ComputedGridTrackInfo& aTrackInfo);

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
  already_AddRefed<CSSValue> DoGetMozOsxFontSmoothing();

  /* Grid properties */
  already_AddRefed<CSSValue> DoGetGridTemplateColumns();
  already_AddRefed<CSSValue> DoGetGridTemplateRows();

  /* StyleImageLayer properties */
  already_AddRefed<CSSValue> DoGetImageLayerPosition(
      const nsStyleImageLayers& aLayers);

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
  already_AddRefed<CSSValue> DoGetMarginTop();
  already_AddRefed<CSSValue> DoGetMarginBottom();
  already_AddRefed<CSSValue> DoGetMarginLeft();
  already_AddRefed<CSSValue> DoGetMarginRight();

  /* Display properties */
  already_AddRefed<CSSValue> DoGetTransform();
  already_AddRefed<CSSValue> DoGetTransformOrigin();
  already_AddRefed<CSSValue> DoGetPerspectiveOrigin();

  // For working around a MSVC bug. See related comment in
  // GenerateComputedDOMStyleGenerated.py.
  already_AddRefed<CSSValue> DummyGetter();

  /* Helper functions */
  void SetValueToPosition(const mozilla::Position& aPosition,
                          nsDOMCSSValueList* aValueList);

  void SetValueFromFitContentFunction(nsROCSSPrimitiveValue* aValue,
                                      const mozilla::LengthPercentage&);

  void SetValueToSize(nsROCSSPrimitiveValue* aValue, const mozilla::StyleSize&);

  void SetValueToLengthPercentageOrAuto(nsROCSSPrimitiveValue* aValue,
                                        const LengthPercentageOrAuto&,
                                        bool aClampNegativeCalc);

  void SetValueToLengthPercentage(nsROCSSPrimitiveValue* aValue,
                                  const LengthPercentage&,
                                  bool aClampNegativeCalc);

  void SetValueToMaxSize(nsROCSSPrimitiveValue* aValue,
                         const mozilla::StyleMaxSize&);

  bool GetCBContentWidth(nscoord& aWidth);
  bool GetCBContentHeight(nscoord& aHeight);
  bool GetCBPaddingRectWidth(nscoord& aWidth);
  bool GetCBPaddingRectHeight(nscoord& aHeight);
  bool GetScrollFrameContentWidth(nscoord& aWidth);
  bool GetScrollFrameContentHeight(nscoord& aHeight);
  bool GetFrameBorderRectWidth(nscoord& aWidth);
  bool GetFrameBorderRectHeight(nscoord& aHeight);

  // Find out if we can safely skip flushing (i.e. pending restyles do not
  // affect our element).
  bool NeedsToFlushStyle(nsCSSPropertyID) const;
  // Find out if we need to flush layout of the document, depending on the
  // property that was requested.
  bool NeedsToFlushLayout(nsCSSPropertyID) const;
  // Flushes the given document, which must be our document, and potentially the
  // mElement's document.
  void Flush(Document&, mozilla::FlushType);
  nsIFrame* GetOuterFrame() const;

  static ComputedStyleMap* GetComputedStyleMap();

  // We don't really have a good immutable representation of "presentation".
  // Given the way GetComputedStyle is currently used, we should just grab the
  // presshell, if any, from the document.
  mozilla::WeakPtr<mozilla::dom::Document> mDocumentWeak;
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
  RefPtr<const ComputedStyle> mComputedStyle;

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

  PseudoStyleType mPseudo;
  RefPtr<nsAtom> mFunctionalPseudoParameter;

  /* The kind of styles we should be returning. */
  StyleType mStyleType;

  /* Whether for legacy reasons we return an empty style (when an unknown
   * pseudo-element is specified) */
  AlwaysReturnEmptyStyle mAlwaysReturnEmpty;

  /**
   * The nsComputedDOMStyle generation at the time we last resolved a style
   * context and stored it in mComputedStyle, and the pres shell we got the
   * style from. Should only be used together.
   */
  uint64_t mComputedStyleGeneration = 0;

  uint32_t mPresShellId = 0;

  bool mExposeVisitedStyle = false;

  /**
   * Whether we resolved a ComputedStyle last time we called
   * UpdateCurrentStyleSources.  Initially false.
   */
  bool mResolvedComputedStyle = false;

#ifdef DEBUG
  bool mFlushedPendingReflows = false;
#endif

  friend struct ComputedStyleMap;
};

already_AddRefed<nsComputedDOMStyle> NS_NewComputedDOMStyle(
    mozilla::dom::Element*, const nsAString& aPseudoElt,
    mozilla::dom::Document*, nsComputedDOMStyle::StyleType,
    mozilla::ErrorResult&);

#endif /* nsComputedDOMStyle_h__ */
