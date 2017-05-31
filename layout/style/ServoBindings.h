/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindings_h
#define mozilla_ServoBindings_h

#include <stdint.h>

#include "mozilla/ServoTypes.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/css/URLMatchingFunction.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/ComputedTimingFunction.h"
#include "nsChangeHint.h"
#include "nsCSSPseudoClasses.h"
#include "nsStyleStruct.h"

/*
 * API for Servo to access Gecko data structures. This file must compile as valid
 * C code in order for the binding generator to parse it.
 *
 * Functions beginning with Gecko_ are implemented in Gecko and invoked from Servo.
 * Functions beginning with Servo_ are implemented in Servo and invoked from Gecko.
 */

class nsIAtom;
class nsIPrincipal;
class nsIURI;
struct nsFont;
namespace mozilla {
  class FontFamilyList;
  enum FontFamilyType : uint32_t;
  enum class CSSPseudoElementType : uint8_t;
  struct Keyframe;
  enum Side;
  struct StyleTransition;
  namespace css {
    struct URLValue;
    struct ImageValue;
  };
  namespace dom {
    enum class IterationCompositeOperation : uint8_t;
  };
  enum class UpdateAnimationsTasks : uint8_t;
  struct LangGroupFontPrefs;
  class ServoStyleSheet;
  class ServoElementSnapshotTable;
}
using mozilla::FontFamilyList;
using mozilla::FontFamilyType;
using mozilla::ServoElementSnapshot;
class nsCSSCounterStyleRule;
class nsCSSFontFaceRule;
struct nsMediaFeature;
struct nsStyleList;
struct nsStyleImage;
struct nsStyleGradientStop;
class nsStyleGradient;
class nsStyleCoord;
struct nsStyleDisplay;

#define NS_DECL_THREADSAFE_FFI_REFCOUNTING(class_, name_)                     \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr);                    \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr);
#define NS_IMPL_THREADSAFE_FFI_REFCOUNTING(class_, name_)                     \
  static_assert(class_::HasThreadSafeRefCnt::value,                           \
                "NS_DECL_THREADSAFE_FFI_REFCOUNTING can only be used with "   \
                "classes that have thread-safe refcounting");                 \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr)                     \
  { NS_ADDREF(aPtr); }                                                        \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr)                    \
  { NS_RELEASE(aPtr); }

#define NS_DECL_FFI_REFCOUNTING(class_, name_)  \
  void Gecko_##name_##_AddRef(class_* aPtr);    \
  void Gecko_##name_##_Release(class_* aPtr);
#define NS_IMPL_FFI_REFCOUNTING(class_, name_)                    \
  void Gecko_##name_##_AddRef(class_* aPtr)                       \
    { MOZ_ASSERT(NS_IsMainThread()); NS_ADDREF(aPtr); }           \
  void Gecko_##name_##_Release(class_* aPtr)                      \
    { MOZ_ASSERT(NS_IsMainThread()); NS_RELEASE(aPtr); }

#define DEFINE_ARRAY_TYPE_FOR(type_)                                \
  struct nsTArrayBorrowed_##type_ {                                 \
    nsTArray<type_>* mArray;                                        \
    MOZ_IMPLICIT nsTArrayBorrowed_##type_(nsTArray<type_>* aArray)  \
      : mArray(aArray) {}                                           \
  }
DEFINE_ARRAY_TYPE_FOR(uintptr_t);
#undef DEFINE_ARRAY_TYPE_FOR

extern "C" {

class ServoBundledURI
{
public:
  already_AddRefed<mozilla::css::URLValue> IntoCssUrl();
  const uint8_t* mURLString;
  uint32_t mURLStringLength;
  mozilla::URLExtraData* mExtraData;
};

struct FontSizePrefs
{
  void CopyFrom(const mozilla::LangGroupFontPrefs&);
  nscoord mDefaultVariableSize;
  nscoord mDefaultFixedSize;
  nscoord mDefaultSerifSize;
  nscoord mDefaultSansSerifSize;
  nscoord mDefaultMonospaceSize;
  nscoord mDefaultCursiveSize;
  nscoord mDefaultFantasySize;
};

// DOM Traversal.
uint32_t Gecko_ChildrenCount(RawGeckoNodeBorrowed node);
bool Gecko_NodeIsElement(RawGeckoNodeBorrowed node);
bool Gecko_IsInDocument(RawGeckoNodeBorrowed node);
bool Gecko_FlattenedTreeParentIsParent(RawGeckoNodeBorrowed node);
bool Gecko_IsSignificantChild(RawGeckoNodeBorrowed node,
                              bool text_is_significant,
                              bool whitespace_is_significant);
RawGeckoNodeBorrowedOrNull Gecko_GetParentNode(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetFirstChild(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetLastChild(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetPrevSibling(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetNextSibling(RawGeckoNodeBorrowed node);
RawGeckoElementBorrowedOrNull Gecko_GetFirstChildElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetLastChildElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetPrevSiblingElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetNextSiblingElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetDocumentElement(RawGeckoDocumentBorrowed document);
void Gecko_LoadStyleSheet(mozilla::css::Loader* loader,
                          mozilla::ServoStyleSheet* parent,
                          RawServoStyleSheetBorrowed child_sheet,
                          RawGeckoURLExtraData* base_url_data,
                          const uint8_t* url_bytes,
                          uint32_t url_length,
                          RawServoMediaListStrong media_list);

// By default, Servo walks the DOM by traversing the siblings of the DOM-view
// first child. This generally works, but misses anonymous children, which we
// want to traverse during styling. To support these cases, we create an
// optional heap-allocated iterator for nodes that need it. If the creation
// method returns null, Servo falls back to the aforementioned simpler (and
// faster) sibling traversal.
StyleChildrenIteratorOwnedOrNull Gecko_MaybeCreateStyleChildrenIterator(RawGeckoNodeBorrowed node);
void Gecko_DropStyleChildrenIterator(StyleChildrenIteratorOwned it);
RawGeckoNodeBorrowedOrNull Gecko_GetNextStyleChild(StyleChildrenIteratorBorrowedMut it);

// Selector Matching.
uint64_t Gecko_ElementState(RawGeckoElementBorrowed element);
bool Gecko_IsTextNode(RawGeckoNodeBorrowed node);
bool Gecko_IsRootElement(RawGeckoElementBorrowed element);
bool Gecko_MatchesElement(mozilla::CSSPseudoClassType type, RawGeckoElementBorrowed element);
nsIAtom* Gecko_LocalName(RawGeckoElementBorrowed element);
nsIAtom* Gecko_Namespace(RawGeckoElementBorrowed element);
nsIAtom* Gecko_GetElementId(RawGeckoElementBorrowed element);

nsIAtom* Gecko_GetXMLLangValue(RawGeckoElementBorrowed element);

// Attributes.
#define SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)  \
  nsIAtom* prefix_##AtomAttrValue(implementor_ element, nsIAtom* attribute);  \
  bool prefix_##HasAttr(implementor_ element, nsIAtom* ns, nsIAtom* name);    \
  bool prefix_##AttrEquals(implementor_ element, nsIAtom* ns, nsIAtom* name,  \
                           nsIAtom* str, bool ignoreCase);                    \
  bool prefix_##AttrDashEquals(implementor_ element, nsIAtom* ns,             \
                               nsIAtom* name, nsIAtom* str);                  \
  bool prefix_##AttrIncludes(implementor_ element, nsIAtom* ns,               \
                             nsIAtom* name, nsIAtom* str);                    \
  bool prefix_##AttrHasSubstring(implementor_ element, nsIAtom* ns,           \
                                 nsIAtom* name, nsIAtom* str);                \
  bool prefix_##AttrHasPrefix(implementor_ element, nsIAtom* ns,              \
                              nsIAtom* name, nsIAtom* str);                   \
  bool prefix_##AttrHasSuffix(implementor_ element, nsIAtom* ns,              \
                              nsIAtom* name, nsIAtom* str);                   \
  uint32_t prefix_##ClassOrClassList(implementor_ element, nsIAtom** class_,  \
                                     nsIAtom*** classList);

SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_, RawGeckoElementBorrowed)
SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_Snapshot,
                                              const ServoElementSnapshot*)

#undef SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS

// Style attributes.
RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetStyleAttrDeclarationBlock(RawGeckoElementBorrowed element);
void Gecko_UnsetDirtyStyleAttr(RawGeckoElementBorrowed element);
RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetHTMLPresentationAttrDeclarationBlock(RawGeckoElementBorrowed element);
RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetExtraContentStyleDeclarations(RawGeckoElementBorrowed element);

// Animations
bool
Gecko_GetAnimationRule(RawGeckoElementBorrowed aElementOrPseudo,
                       mozilla::EffectCompositor::CascadeLevel aCascadeLevel,
                       RawServoAnimationValueMapBorrowedMut aAnimationValues);
RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetSMILOverrideDeclarationBlock(RawGeckoElementBorrowed element);
bool Gecko_StyleAnimationsEquals(RawGeckoStyleAnimationListBorrowed,
                                 RawGeckoStyleAnimationListBorrowed);
void Gecko_UpdateAnimations(RawGeckoElementBorrowed aElementOrPseudo,
                            ServoComputedValuesBorrowedOrNull aOldComputedValues,
                            ServoComputedValuesBorrowedOrNull aComputedValues,
                            ServoComputedValuesBorrowedOrNull aParentComputedValues,
                            mozilla::UpdateAnimationsTasks aTasks);
bool Gecko_ElementHasAnimations(RawGeckoElementBorrowed aElementOrPseudo);
bool Gecko_ElementHasCSSAnimations(RawGeckoElementBorrowed aElementOrPseudo);
bool Gecko_ElementHasCSSTransitions(RawGeckoElementBorrowed aElementOrPseudo);
size_t Gecko_ElementTransitions_Length(RawGeckoElementBorrowed aElementOrPseudo);
nsCSSPropertyID Gecko_ElementTransitions_PropertyAt(
  RawGeckoElementBorrowed aElementOrPseudo,
  size_t aIndex);
RawServoAnimationValueBorrowedOrNull Gecko_ElementTransitions_EndValueAt(
  RawGeckoElementBorrowed aElementOrPseudo,
  size_t aIndex);
double Gecko_GetProgressFromComputedTiming(RawGeckoComputedTimingBorrowed aComputedTiming);
double Gecko_GetPositionInSegment(
  RawGeckoAnimationPropertySegmentBorrowed aSegment,
  double aProgress,
  mozilla::ComputedTimingFunction::BeforeFlag aBeforeFlag);
// Get servo's AnimationValue for |aProperty| from the cached base style
// |aBaseStyles|.
// |aBaseStyles| is nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>.
// We use void* to avoid exposing nsRefPtrHashtable in FFI.
RawServoAnimationValueBorrowedOrNull Gecko_AnimationGetBaseStyle(
  void* aBaseStyles,
  nsCSSPropertyID aProperty);
void Gecko_StyleTransition_SetUnsupportedProperty(
  mozilla::StyleTransition* aTransition,
  nsIAtom* aAtom);

// Atoms.
nsIAtom* Gecko_Atomize(const char* aString, uint32_t aLength);
nsIAtom* Gecko_Atomize16(const nsAString* aString);
void Gecko_AddRefAtom(nsIAtom* aAtom);
void Gecko_ReleaseAtom(nsIAtom* aAtom);
const uint16_t* Gecko_GetAtomAsUTF16(nsIAtom* aAtom, uint32_t* aLength);
bool Gecko_AtomEqualsUTF8(nsIAtom* aAtom, const char* aString, uint32_t aLength);
bool Gecko_AtomEqualsUTF8IgnoreCase(nsIAtom* aAtom, const char* aString, uint32_t aLength);

// Border style
void Gecko_EnsureMozBorderColors(nsStyleBorder* aBorder);
void Gecko_ClearMozBorderColors(nsStyleBorder* aBorder, mozilla::Side aSide);
void Gecko_AppendMozBorderColors(nsStyleBorder* aBorder, mozilla::Side aSide,
                                 nscolor aColor);
void Gecko_CopyMozBorderColors(nsStyleBorder* aDest, const nsStyleBorder* aSrc,
                               mozilla::Side aSide);

// Font style
void Gecko_FontFamilyList_Clear(FontFamilyList* aList);
void Gecko_FontFamilyList_AppendNamed(FontFamilyList* aList, nsIAtom* aName, bool aQuoted);
void Gecko_FontFamilyList_AppendGeneric(FontFamilyList* list, FontFamilyType familyType);
void Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src);
// will not run destructors on dst, give it uninitialized memory
// font_id is LookAndFeel::FontID
void Gecko_nsFont_InitSystem(nsFont* dst, int32_t font_id,
                             const nsStyleFont* font, RawGeckoPresContextBorrowed pres_context);
void Gecko_nsFont_Destroy(nsFont* dst);

// Visibility style
void Gecko_SetImageOrientation(nsStyleVisibility* aVisibility,
                               double aRadians,
                               bool aFlip);
void Gecko_SetImageOrientationAsFromImage(nsStyleVisibility* aVisibility);
void Gecko_CopyImageOrientationFrom(nsStyleVisibility* aDst,
                                    const nsStyleVisibility* aSrc);

// Counter style.
// This function takes an already addrefed nsIAtom
void Gecko_SetCounterStyleToName(mozilla::CounterStylePtr* ptr, nsIAtom* name);
void Gecko_SetCounterStyleToSymbols(mozilla::CounterStylePtr* ptr,
                                    uint8_t symbols_type,
                                    nsACString const* const* symbols,
                                    uint32_t symbols_count);
void Gecko_SetCounterStyleToString(mozilla::CounterStylePtr* ptr,
                                   const nsACString* symbol);
void Gecko_CopyCounterStyle(mozilla::CounterStylePtr* dst,
                            const mozilla::CounterStylePtr* src);

// background-image style.
void Gecko_SetNullImageValue(nsStyleImage* image);
void Gecko_SetGradientImageValue(nsStyleImage* image, nsStyleGradient* gradient);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::ImageValue, ImageValue);
mozilla::css::ImageValue* Gecko_ImageValue_Create(ServoBundledURI aURI);
void Gecko_SetLayerImageImageValue(nsStyleImage* image,
                                   mozilla::css::ImageValue* aImageValue);

void Gecko_SetImageElement(nsStyleImage* image, nsIAtom* atom);
void Gecko_CopyImageValueFrom(nsStyleImage* image, const nsStyleImage* other);
void Gecko_InitializeImageCropRect(nsStyleImage* image);

nsStyleGradient* Gecko_CreateGradient(uint8_t shape,
                                      uint8_t size,
                                      bool repeating,
                                      bool legacy_syntax,
                                      uint32_t stops);

// list-style-image style.
void Gecko_SetListStyleImageNone(nsStyleList* style_struct);
void Gecko_SetListStyleImageImageValue(nsStyleList* style_struct,
                                  mozilla::css::ImageValue* aImageValue);
void Gecko_CopyListStyleImageFrom(nsStyleList* dest, const nsStyleList* src);

// cursor style.
void Gecko_SetCursorArrayLength(nsStyleUserInterface* ui, size_t len);
void Gecko_SetCursorImageValue(nsCursorImage* aCursor,
                               mozilla::css::ImageValue* aImageValue);
void Gecko_CopyCursorArrayFrom(nsStyleUserInterface* dest,
                               const nsStyleUserInterface* src);

void Gecko_SetContentDataImageValue(nsStyleContentData* aList,
                                    mozilla::css::ImageValue* aImageValue);
nsStyleContentData::CounterFunction* Gecko_SetCounterFunction(
    nsStyleContentData* content_data, nsStyleContentType type);

// Dirtiness tracking.
uint32_t Gecko_GetNodeFlags(RawGeckoNodeBorrowed node);
void Gecko_SetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);
void Gecko_UnsetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);
void Gecko_SetOwnerDocumentNeedsStyleFlush(RawGeckoElementBorrowed element);

// Incremental restyle.
// Also, we might want a ComputedValues to ComputedValues API for animations?
// Not if we do them in Gecko...
nsStyleContext* Gecko_GetStyleContext(RawGeckoElementBorrowed element,
                                      nsIAtom* aPseudoTagOrNull);
mozilla::CSSPseudoElementType Gecko_GetImplementedPseudo(RawGeckoElementBorrowed element);
nsChangeHint Gecko_CalcStyleDifference(nsStyleContext* oldstyle,
                                       ServoComputedValuesBorrowed newstyle,
                                       bool* any_style_changed);
nsChangeHint Gecko_HintsHandledForDescendants(nsChangeHint aHint);

// Get an element snapshot for a given element from the table.
const ServoElementSnapshot*
Gecko_GetElementSnapshot(const mozilla::ServoElementSnapshotTable* table,
                         RawGeckoElementBorrowed element);

void Gecko_DropElementSnapshot(ServoElementSnapshotOwned snapshot);

// `array` must be an nsTArray
// If changing this signature, please update the
// friend function declaration in nsTArray.h
void Gecko_EnsureTArrayCapacity(void* array, size_t capacity, size_t elem_size);

// Same here, `array` must be an nsTArray<T>, for some T.
//
// Important note: Only valid for POD types, since destructors won't be run
// otherwise. This is ensured with rust traits for the relevant structs.
void Gecko_ClearPODTArray(void* array, size_t elem_size, size_t elem_align);

void Gecko_ResizeTArrayForStrings(nsTArray<nsString>* array, uint32_t length);

void Gecko_SetStyleGridTemplateArrayLengths(nsStyleGridTemplate* grid_template,
                                            uint32_t track_sizes);

void Gecko_CopyStyleGridTemplateValues(nsStyleGridTemplate* grid_template,
                                       const nsStyleGridTemplate* other);

mozilla::css::GridTemplateAreasValue* Gecko_NewGridTemplateAreasValue(uint32_t areas,
                                                                      uint32_t templates,
                                                                      uint32_t columns);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::GridTemplateAreasValue, GridTemplateAreasValue);

// Clear the mContents, mCounterIncrements, or mCounterResets field in nsStyleContent. This is
// needed to run the destructors, otherwise we'd leak the images, strings, and whatnot.
void Gecko_ClearAndResizeStyleContents(nsStyleContent* content,
                                       uint32_t how_many);
void Gecko_ClearAndResizeCounterIncrements(nsStyleContent* content,
                                           uint32_t how_many);
void Gecko_ClearAndResizeCounterResets(nsStyleContent* content,
                                       uint32_t how_many);
void Gecko_CopyStyleContentsFrom(nsStyleContent* content, const nsStyleContent* other);
void Gecko_CopyCounterResetsFrom(nsStyleContent* content, const nsStyleContent* other);
void Gecko_CopyCounterIncrementsFrom(nsStyleContent* content, const nsStyleContent* other);

void Gecko_EnsureImageLayersLength(nsStyleImageLayers* layers, size_t len,
                                   nsStyleImageLayers::LayerType layer_type);

void Gecko_EnsureStyleAnimationArrayLength(void* array, size_t len);
void Gecko_EnsureStyleTransitionArrayLength(void* array, size_t len);

void Gecko_ClearWillChange(nsStyleDisplay* display, size_t length);
void Gecko_AppendWillChange(nsStyleDisplay* display, nsIAtom* atom);
void Gecko_CopyWillChangeFrom(nsStyleDisplay* dest, nsStyleDisplay* src);

// Searches from the beginning of |keyframes| for a Keyframe object with the
// specified offset and timing function. If none is found, a new Keyframe object
// with the specified |offset| and |timingFunction| will be prepended to
// |keyframes|.
//
// @param keyframes  An array of Keyframe objects, sorted by offset.
//                   The first Keyframe in the array, if any, MUST have an
//                   offset greater than or equal to |offset|.
// @param offset  The offset to search for, or, if no suitable Keyframe is
//                found, the offset to use for the created Keyframe.
//                Must be a floating point number in the range [0.0, 1.0].
// @param timingFunction  The timing function to match, or, if no suitable
//                        Keyframe is found, to set on the created Keyframe.
//
// @returns  The matching or created Keyframe.
mozilla::Keyframe* Gecko_GetOrCreateKeyframeAtStart(
  RawGeckoKeyframeListBorrowedMut keyframes,
  float offset,
  const nsTimingFunction* timingFunction);

// As with Gecko_GetOrCreateKeyframeAtStart except that this method will search
// from the beginning of |keyframes| for a Keyframe with matching timing
// function and an offset of 0.0.
// Furthermore, if a matching Keyframe is not found, a new Keyframe will be
// inserted after the *last* Keyframe in |keyframes| with offset 0.0.
mozilla::Keyframe* Gecko_GetOrCreateInitialKeyframe(
  RawGeckoKeyframeListBorrowedMut keyframes,
  const nsTimingFunction* timingFunction);

// As with Gecko_GetOrCreateKeyframeAtStart except that this method will search
// from the *end* of |keyframes| for a Keyframe with matching timing function
// and an offset of 1.0. If a matching Keyframe is not found, a new Keyframe
// will be appended to the end of |keyframes|.
mozilla::Keyframe* Gecko_GetOrCreateFinalKeyframe(
  RawGeckoKeyframeListBorrowedMut keyframes,
  const nsTimingFunction* timingFunction);

// Clean up pointer-based coordinates
void Gecko_ResetStyleCoord(nsStyleUnit* unit, nsStyleUnion* value);

// Set an nsStyleCoord to a computed `calc()` value
void Gecko_SetStyleCoordCalcValue(nsStyleUnit* unit, nsStyleUnion* value, nsStyleCoord::CalcValue calc);

void Gecko_CopyShapeSourceFrom(mozilla::StyleShapeSource* dst, const mozilla::StyleShapeSource* src);

void Gecko_DestroyShapeSource(mozilla::StyleShapeSource* shape);
mozilla::StyleBasicShape* Gecko_NewBasicShape(mozilla::StyleBasicShapeType type);
void Gecko_StyleShapeSource_SetURLValue(mozilla::StyleShapeSource* shape, ServoBundledURI uri);

void Gecko_ResetFilters(nsStyleEffects* effects, size_t new_len);
void Gecko_CopyFiltersFrom(nsStyleEffects* aSrc, nsStyleEffects* aDest);
void Gecko_nsStyleFilter_SetURLValue(nsStyleFilter* effects, ServoBundledURI uri);

void Gecko_nsStyleSVGPaint_CopyFrom(nsStyleSVGPaint* dest, const nsStyleSVGPaint* src);
void Gecko_nsStyleSVGPaint_SetURLValue(nsStyleSVGPaint* paint, ServoBundledURI uri);
void Gecko_nsStyleSVGPaint_Reset(nsStyleSVGPaint* paint);

void Gecko_nsStyleSVG_SetDashArrayLength(nsStyleSVG* svg, uint32_t len);
void Gecko_nsStyleSVG_CopyDashArray(nsStyleSVG* dst, const nsStyleSVG* src);
void Gecko_nsStyleSVG_SetContextPropertiesLength(nsStyleSVG* svg, uint32_t len);
void Gecko_nsStyleSVG_CopyContextProperties(nsStyleSVG* dst, const nsStyleSVG* src);

mozilla::css::URLValue* Gecko_NewURLValue(ServoBundledURI uri);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::URLValue, CSSURLValue);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(RawGeckoURLExtraData, URLExtraData);

void Gecko_FillAllBackgroundLists(nsStyleImageLayers* layers, uint32_t max_len);
void Gecko_FillAllMaskLists(nsStyleImageLayers* layers, uint32_t max_len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

nsCSSShadowArray* Gecko_NewCSSShadowArray(uint32_t len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSShadowArray, CSSShadowArray);

nsStyleQuoteValues* Gecko_NewStyleQuoteValues(uint32_t len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleQuoteValues, QuoteValues);

nsCSSValueSharedList* Gecko_NewCSSValueSharedList(uint32_t len);
nsCSSValueSharedList* Gecko_NewNoneTransform();

// Getter for nsCSSValue
nsCSSValueBorrowedMut Gecko_CSSValue_GetArrayItem(nsCSSValueBorrowedMut css_value, int32_t index);
// const version of the above function.
nsCSSValueBorrowed Gecko_CSSValue_GetArrayItemConst(nsCSSValueBorrowed css_value, int32_t index);
nscoord Gecko_CSSValue_GetAbsoluteLength(nsCSSValueBorrowed css_value);
nsCSSKeyword Gecko_CSSValue_GetKeyword(nsCSSValueBorrowed aCSSValue);
float Gecko_CSSValue_GetNumber(nsCSSValueBorrowed css_value);
float Gecko_CSSValue_GetPercentage(nsCSSValueBorrowed css_value);
nsStyleCoord::CalcValue Gecko_CSSValue_GetCalc(nsCSSValueBorrowed aCSSValue);

void Gecko_CSSValue_SetAbsoluteLength(nsCSSValueBorrowedMut css_value, nscoord len);
void Gecko_CSSValue_SetNumber(nsCSSValueBorrowedMut css_value, float number);
void Gecko_CSSValue_SetKeyword(nsCSSValueBorrowedMut css_value, nsCSSKeyword keyword);
void Gecko_CSSValue_SetPercentage(nsCSSValueBorrowedMut css_value, float percent);
void Gecko_CSSValue_SetCalc(nsCSSValueBorrowedMut css_value, nsStyleCoord::CalcValue calc);
void Gecko_CSSValue_SetFunction(nsCSSValueBorrowedMut css_value, int32_t len);
void Gecko_CSSValue_SetString(nsCSSValueBorrowedMut css_value,
                              const uint8_t* string, uint32_t len, nsCSSUnit unit);
void Gecko_CSSValue_SetStringFromAtom(nsCSSValueBorrowedMut css_value,
                                      nsIAtom* atom, nsCSSUnit unit);
// Take an addrefed nsIAtom and set it to the nsCSSValue
void Gecko_CSSValue_SetAtomIdent(nsCSSValueBorrowedMut css_value, nsIAtom* atom);
void Gecko_CSSValue_SetArray(nsCSSValueBorrowedMut css_value, int32_t len);
void Gecko_CSSValue_SetURL(nsCSSValueBorrowedMut css_value, ServoBundledURI uri);
void Gecko_CSSValue_SetInt(nsCSSValueBorrowedMut css_value, int32_t integer, nsCSSUnit unit);
void Gecko_CSSValue_SetPair(nsCSSValueBorrowedMut css_value,
                            nsCSSValueBorrowed xvalue, nsCSSValueBorrowed yvalue);
void Gecko_CSSValue_SetList(nsCSSValueBorrowedMut css_value, uint32_t len);
void Gecko_CSSValue_SetPairList(nsCSSValueBorrowedMut css_value, uint32_t len);
void Gecko_CSSValue_Drop(nsCSSValueBorrowedMut css_value);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSValueSharedList, CSSValueSharedList);
bool Gecko_PropertyId_IsPrefEnabled(nsCSSPropertyID id);

void Gecko_nsStyleFont_SetLang(nsStyleFont* font, nsIAtom* atom);
void Gecko_nsStyleFont_CopyLangFrom(nsStyleFont* aFont, const nsStyleFont* aSource);
void Gecko_nsStyleFont_FixupNoneGeneric(nsStyleFont* font,
                                        RawGeckoPresContextBorrowed pres_context);
void Gecko_nsStyleFont_FixupMinFontSize(nsStyleFont* font,
                                        RawGeckoPresContextBorrowed pres_context);
FontSizePrefs Gecko_GetBaseSize(nsIAtom* lang);

struct GeckoFontMetrics
{
  nscoord mChSize;
  nscoord mXSize;
};

GeckoFontMetrics Gecko_GetFontMetrics(RawGeckoPresContextBorrowed pres_context,
                                      bool is_vertical,
                                      const nsStyleFont* font,
                                      nscoord font_size,
                                      bool use_user_font_set);
int32_t Gecko_GetAppUnitsPerPhysicalInch(RawGeckoPresContextBorrowed pres_context);
void InitializeServo();
void ShutdownServo();
void AssertIsMainThreadOrServoLangFontPrefsCacheLocked();

const nsMediaFeature* Gecko_GetMediaFeatures();
nsCSSKeyword Gecko_LookupCSSKeyword(const uint8_t* string, uint32_t len);
const char* Gecko_CSSKeywordString(nsCSSKeyword keyword, uint32_t* len);

// Font face rule
// Creates and returns a new (already-addrefed) nsCSSFontFaceRule object.
nsCSSFontFaceRule* Gecko_CSSFontFaceRule_Create(uint32_t line, uint32_t column);
void Gecko_CSSFontFaceRule_GetCssText(const nsCSSFontFaceRule* rule, nsAString* result);
NS_DECL_FFI_REFCOUNTING(nsCSSFontFaceRule, CSSFontFaceRule);

// Counter style rule
// Creates and returns a new (already-addrefed) nsCSSCounterStyleRule object.
nsCSSCounterStyleRule* Gecko_CSSCounterStyle_Create(nsIAtom* name);
void Gecko_CSSCounterStyle_GetCssText(const nsCSSCounterStyleRule* rule, nsAString* result);
NS_DECL_FFI_REFCOUNTING(nsCSSCounterStyleRule, CSSCounterStyleRule);

RawGeckoElementBorrowedOrNull Gecko_GetBody(RawGeckoPresContextBorrowed pres_context);

// We use an int32_t here instead of a LookAndFeel::ColorID
// because forward-declaring a nested enum/struct is impossible
nscolor Gecko_GetLookAndFeelSystemColor(int32_t color_id,
                                        RawGeckoPresContextBorrowed pres_context);

bool Gecko_MatchStringArgPseudo(RawGeckoElementBorrowed element,
                                mozilla::CSSPseudoClassType type,
                                const char16_t* ident,
                                bool* set_slow_selector);

void Gecko_AddPropertyToSet(nsCSSPropertyIDSetBorrowedMut, nsCSSPropertyID);

// Style-struct management.
#define STYLE_STRUCT(name, checkdata_cb)                                       \
  void Gecko_Construct_Default_nsStyle##name(                                  \
    nsStyle##name* ptr,                                                        \
    RawGeckoPresContextBorrowed pres_context);                                 \
  void Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,                   \
                                         const nsStyle##name* other);          \
  void Gecko_Destroy_nsStyle##name(nsStyle##name* ptr);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

void Gecko_Construct_nsStyleVariables(nsStyleVariables* ptr);

void Gecko_RegisterProfilerThread(const char* name);
void Gecko_UnregisterProfilerThread();

bool Gecko_DocumentRule_UseForPresentation(RawGeckoPresContextBorrowed,
                                           const nsACString* aPattern,
                                           mozilla::css::URLMatchingFunction aURLMatchingFunction);

// Allocator hinting.
void Gecko_SetJemallocThreadLocalArena(bool enabled);

// Pseudo-element flags.
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  const uint32_t SERVO_CSS_PSEUDO_ELEMENT_FLAGS_##name_ = flags_;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

#define SERVO_BINDING_FUNC(name_, return_, ...) return_ name_(__VA_ARGS__);
#include "mozilla/ServoBindingList.h"
#undef SERVO_BINDING_FUNC

} // extern "C"

#endif // mozilla_ServoBindings_h
