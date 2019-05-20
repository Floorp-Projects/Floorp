/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Servo to call into Gecko */

#ifndef mozilla_GeckoBindings_h
#define mozilla_GeckoBindings_h

#include <stdint.h>

#include "mozilla/ServoTypes.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/css/DocumentMatchingFunction.h"
#include "mozilla/css/SheetLoadData.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/ComputedTimingFunction.h"
#include "mozilla/PreferenceSheet.h"
#include "nsStyleStruct.h"

class nsAtom;
class nsIURI;
class nsSimpleContentList;
struct nsFont;

namespace mozilla {
class ComputedStyle;
class SeenPtrs;
class ServoElementSnapshot;
class ServoElementSnapshotTable;
class SharedFontList;
class StyleSheet;
enum class PseudoStyleType : uint8_t;
enum class PointerCapabilities : uint8_t;
enum class UpdateAnimationsTasks : uint8_t;
struct FontFamilyName;
struct Keyframe;

namespace css {
class LoaderReusableStyleSheets;
}
}  // namespace mozilla

#ifdef NIGHTLY_BUILD
const bool GECKO_IS_NIGHTLY = true;
#else
const bool GECKO_IS_NIGHTLY = false;
#endif

#define NS_DECL_THREADSAFE_FFI_REFCOUNTING(class_, name_)  \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr); \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr);
#define NS_IMPL_THREADSAFE_FFI_REFCOUNTING(class_, name_)                      \
  static_assert(class_::HasThreadSafeRefCnt::value,                            \
                "NS_DECL_THREADSAFE_FFI_REFCOUNTING can only be used with "    \
                "classes that have thread-safe refcounting");                  \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr) { NS_ADDREF(aPtr); } \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr) { NS_RELEASE(aPtr); }

extern "C" {

// Debugging stuff.
void Gecko_Element_DebugListAttributes(const mozilla::dom::Element*,
                                       nsCString*);

void Gecko_Snapshot_DebugListAttributes(const mozilla::ServoElementSnapshot*,
                                        nsCString*);

bool Gecko_IsSignificantChild(const nsINode*, bool whitespace_is_significant);

const nsINode* Gecko_GetLastChild(const nsINode*);
const nsINode* Gecko_GetPreviousSibling(const nsINode*);

const nsINode* Gecko_GetFlattenedTreeParentNode(const nsINode*);
const mozilla::dom::Element* Gecko_GetBeforeOrAfterPseudo(
    const mozilla::dom::Element*, bool is_before);
const mozilla::dom::Element* Gecko_GetMarkerPseudo(
    const mozilla::dom::Element*);

nsTArray<nsIContent*>* Gecko_GetAnonymousContentForElement(
    const mozilla::dom::Element*);
void Gecko_DestroyAnonymousContentList(nsTArray<nsIContent*>* anon_content);

const nsTArray<RefPtr<nsINode>>* Gecko_GetAssignedNodes(
    const mozilla::dom::Element*);

void Gecko_ComputedStyle_Init(mozilla::ComputedStyle* context,
                              const ServoComputedData* values,
                              mozilla::PseudoStyleType pseudo_type);

void Gecko_ComputedStyle_Destroy(mozilla::ComputedStyle* context);

// By default, Servo walks the DOM by traversing the siblings of the DOM-view
// first child. This generally works, but misses anonymous children, which we
// want to traverse during styling. To support these cases, we create an
// optional stack-allocated iterator in aIterator for nodes that need it.
void Gecko_ConstructStyleChildrenIterator(const mozilla::dom::Element*,
                                          mozilla::dom::StyleChildrenIterator*);

void Gecko_DestroyStyleChildrenIterator(mozilla::dom::StyleChildrenIterator*);

const nsINode* Gecko_GetNextStyleChild(mozilla::dom::StyleChildrenIterator*);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::SheetLoadDataHolder,
                                   SheetLoadDataHolder);

void Gecko_StyleSheet_FinishAsyncParse(
    mozilla::css::SheetLoadDataHolder* data,
    mozilla::StyleStrong<RawServoStyleSheetContents> sheet_contents,
    mozilla::StyleOwnedOrNull<StyleUseCounters> use_counters);

mozilla::StyleSheet* Gecko_LoadStyleSheet(
    mozilla::css::Loader* loader, mozilla::StyleSheet* parent,
    mozilla::css::SheetLoadData* parent_load_data,
    mozilla::css::LoaderReusableStyleSheets* reusable_sheets,
    mozilla::StyleStrong<RawServoCssUrlData> url,
    mozilla::StyleStrong<RawServoMediaList> media_list);

void Gecko_LoadStyleSheetAsync(mozilla::css::SheetLoadDataHolder* parent_data,
                               mozilla::StyleStrong<RawServoCssUrlData>,
                               mozilla::StyleStrong<RawServoMediaList>,
                               mozilla::StyleStrong<RawServoImportRule>);

// Selector Matching.
uint64_t Gecko_ElementState(const mozilla::dom::Element*);
bool Gecko_IsRootElement(const mozilla::dom::Element*);

bool Gecko_MatchLang(const mozilla::dom::Element*, nsAtom* override_lang,
                     bool has_override_lang, const char16_t* value);

nsAtom* Gecko_GetXMLLangValue(const mozilla::dom::Element*);

mozilla::dom::Document::DocumentTheme Gecko_GetDocumentLWTheme(
    const mozilla::dom::Document*);

const mozilla::PreferenceSheet::Prefs* Gecko_GetPrefSheetPrefs(
    const mozilla::dom::Document*);

bool Gecko_IsTableBorderNonzero(const mozilla::dom::Element* element);
bool Gecko_IsBrowserFrame(const mozilla::dom::Element* element);

// Attributes.
#define SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)   \
  nsAtom* prefix_##LangValue(implementor_ element);                            \
  bool prefix_##HasAttr(implementor_ element, nsAtom* ns, nsAtom* name);       \
  bool prefix_##AttrEquals(implementor_ element, nsAtom* ns, nsAtom* name,     \
                           nsAtom* str, bool ignoreCase);                      \
  bool prefix_##AttrDashEquals(implementor_ element, nsAtom* ns, nsAtom* name, \
                               nsAtom* str, bool ignore_case);                 \
  bool prefix_##AttrIncludes(implementor_ element, nsAtom* ns, nsAtom* name,   \
                             nsAtom* str, bool ignore_case);                   \
  bool prefix_##AttrHasSubstring(implementor_ element, nsAtom* ns,             \
                                 nsAtom* name, nsAtom* str, bool ignore_case); \
  bool prefix_##AttrHasPrefix(implementor_ element, nsAtom* ns, nsAtom* name,  \
                              nsAtom* str, bool ignore_case);                  \
  bool prefix_##AttrHasSuffix(implementor_ element, nsAtom* ns, nsAtom* name,  \
                              nsAtom* str, bool ignore_case);

bool Gecko_AssertClassAttrValueIsSane(const nsAttrValue*);
const nsAttrValue* Gecko_GetSVGAnimatedClass(const mozilla::dom::Element*);

SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_,
                                              const mozilla::dom::Element*)

SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(
    Gecko_Snapshot, const mozilla::ServoElementSnapshot*)

#undef SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS

// Style attributes.
const mozilla::StyleStrong<RawServoDeclarationBlock>*
Gecko_GetStyleAttrDeclarationBlock(const mozilla::dom::Element* element);

void Gecko_UnsetDirtyStyleAttr(const mozilla::dom::Element* element);

const mozilla::StyleStrong<RawServoDeclarationBlock>*
Gecko_GetHTMLPresentationAttrDeclarationBlock(
    const mozilla::dom::Element* element);

const mozilla::StyleStrong<RawServoDeclarationBlock>*
Gecko_GetExtraContentStyleDeclarations(const mozilla::dom::Element* element);

const mozilla::StyleStrong<RawServoDeclarationBlock>*
Gecko_GetUnvisitedLinkAttrDeclarationBlock(
    const mozilla::dom::Element* element);

const mozilla::StyleStrong<RawServoDeclarationBlock>*
Gecko_GetVisitedLinkAttrDeclarationBlock(const mozilla::dom::Element* element);

const mozilla::StyleStrong<RawServoDeclarationBlock>*
Gecko_GetActiveLinkAttrDeclarationBlock(const mozilla::dom::Element* element);

// Visited handling.

// Returns whether visited styles are enabled for a given document.
bool Gecko_VisitedStylesEnabled(const mozilla::dom::Document*);

// Animations
bool Gecko_GetAnimationRule(
    const mozilla::dom::Element* aElementOrPseudo,
    mozilla::EffectCompositor::CascadeLevel aCascadeLevel,
    RawServoAnimationValueMap* aAnimationValues);

bool Gecko_StyleAnimationsEquals(
    const nsStyleAutoArray<mozilla::StyleAnimation>*,
    const nsStyleAutoArray<mozilla::StyleAnimation>*);

void Gecko_CopyAnimationNames(
    nsStyleAutoArray<mozilla::StyleAnimation>* aDest,
    const nsStyleAutoArray<mozilla::StyleAnimation>* aSrc);

// This function takes an already addrefed nsAtom
void Gecko_SetAnimationName(mozilla::StyleAnimation* aStyleAnimation,
                            nsAtom* aAtom);

void Gecko_UpdateAnimations(const mozilla::dom::Element* aElementOrPseudo,
                            const mozilla::ComputedStyle* aOldComputedValues,
                            const mozilla::ComputedStyle* aComputedValues,
                            mozilla::UpdateAnimationsTasks aTasks);

size_t Gecko_GetAnimationEffectCount(
    const mozilla::dom::Element* aElementOrPseudo);
bool Gecko_ElementHasAnimations(const mozilla::dom::Element* aElementOrPseudo);
bool Gecko_ElementHasCSSAnimations(
    const mozilla::dom::Element* aElementOrPseudo);
bool Gecko_ElementHasCSSTransitions(
    const mozilla::dom::Element* aElementOrPseudo);
bool Gecko_ElementHasWebAnimations(
    const mozilla::dom::Element* aElementOrPseudo);
size_t Gecko_ElementTransitions_Length(
    const mozilla::dom::Element* aElementOrPseudo);

nsCSSPropertyID Gecko_ElementTransitions_PropertyAt(
    const mozilla::dom::Element* aElementOrPseudo, size_t aIndex);

const RawServoAnimationValue* Gecko_ElementTransitions_EndValueAt(
    const mozilla::dom::Element* aElementOrPseudo, size_t aIndex);

double Gecko_GetProgressFromComputedTiming(const mozilla::ComputedTiming*);

double Gecko_GetPositionInSegment(
    const mozilla::AnimationPropertySegment*, double aProgress,
    mozilla::ComputedTimingFunction::BeforeFlag aBeforeFlag);

// Get servo's AnimationValue for |aProperty| from the cached base style
// |aBaseStyles|.
// |aBaseStyles| is nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>.
// We use RawServoAnimationValueTableBorrowed to avoid exposing
// nsRefPtrHashtable in FFI.
const RawServoAnimationValue* Gecko_AnimationGetBaseStyle(
    const RawServoAnimationValueTable* aBaseStyles, nsCSSPropertyID aProperty);

void Gecko_StyleTransition_SetUnsupportedProperty(
    mozilla::StyleTransition* aTransition, nsAtom* aAtom);

// Atoms.
nsAtom* Gecko_Atomize(const char* aString, uint32_t aLength);
nsAtom* Gecko_Atomize16(const nsAString* aString);
void Gecko_AddRefAtom(nsAtom* aAtom);
void Gecko_ReleaseAtom(nsAtom* aAtom);

// Font style
void Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src);

void Gecko_nsTArray_FontFamilyName_AppendNamed(
    nsTArray<mozilla::FontFamilyName>* aNames, nsAtom* aName,
    mozilla::StyleFontFamilyNameSyntax);

void Gecko_nsTArray_FontFamilyName_AppendGeneric(
    nsTArray<mozilla::FontFamilyName>* aNames, mozilla::StyleGenericFontFamily);

// Returns an already-AddRefed SharedFontList with an empty mNames array.
mozilla::SharedFontList* Gecko_SharedFontList_Create();

size_t Gecko_SharedFontList_SizeOfIncludingThis(
    mozilla::SharedFontList* fontlist);

size_t Gecko_SharedFontList_SizeOfIncludingThisIfUnshared(
    mozilla::SharedFontList* fontlist);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::SharedFontList, SharedFontList);

// will not run destructors on dst, give it uninitialized memory
// font_id is LookAndFeel::FontID
void Gecko_nsFont_InitSystem(nsFont* dst, int32_t font_id,
                             const nsStyleFont* font,
                             const mozilla::dom::Document*);

void Gecko_nsFont_Destroy(nsFont* dst);

// The gfxFontFeatureValueSet returned from this function has zero reference.
gfxFontFeatureValueSet* Gecko_ConstructFontFeatureValueSet();

nsTArray<unsigned int>* Gecko_AppendFeatureValueHashEntry(
    gfxFontFeatureValueSet* value_set, nsAtom* family, uint32_t alternate,
    nsAtom* name);

// Font variant alternates
void Gecko_ClearAlternateValues(nsFont* font, size_t length);

void Gecko_AppendAlternateValues(nsFont* font, uint32_t alternate_name,
                                 nsAtom* atom);

void Gecko_CopyAlternateValuesFrom(nsFont* dest, const nsFont* src);

// Visibility style
void Gecko_SetImageOrientation(nsStyleVisibility* aVisibility,
                               uint8_t aOrientation, bool aFlip);

void Gecko_SetImageOrientationAsFromImage(nsStyleVisibility* aVisibility);

void Gecko_CopyImageOrientationFrom(nsStyleVisibility* aDst,
                                    const nsStyleVisibility* aSrc);

// Counter style.
// This function takes an already addrefed nsAtom
void Gecko_SetCounterStyleToName(mozilla::CounterStylePtr* ptr, nsAtom* name);

void Gecko_SetCounterStyleToSymbols(mozilla::CounterStylePtr* ptr,
                                    uint8_t symbols_type,
                                    nsACString const* const* symbols,
                                    uint32_t symbols_count);

void Gecko_SetCounterStyleToString(mozilla::CounterStylePtr* ptr,
                                   const nsACString* symbol);

void Gecko_CopyCounterStyle(mozilla::CounterStylePtr* dst,
                            const mozilla::CounterStylePtr* src);

nsAtom* Gecko_CounterStyle_GetName(const mozilla::CounterStylePtr* ptr);

const mozilla::AnonymousCounterStyle* Gecko_CounterStyle_GetAnonymous(
    const mozilla::CounterStylePtr* ptr);

// background-image style.
void Gecko_SetNullImageValue(nsStyleImage* image);
void Gecko_SetGradientImageValue(nsStyleImage* image,
                                 nsStyleGradient* gradient);

void Gecko_SetLayerImageImageValue(nsStyleImage* image,
                                   mozilla::css::URLValue* image_value);

void Gecko_SetImageElement(nsStyleImage* image, nsAtom* atom);
void Gecko_CopyImageValueFrom(nsStyleImage* image, const nsStyleImage* other);
void Gecko_InitializeImageCropRect(nsStyleImage* image);

nsStyleGradient* Gecko_CreateGradient(uint8_t shape, uint8_t size,
                                      bool repeating, bool legacy_syntax,
                                      bool moz_legacy_syntax, uint32_t stops);

const nsStyleImageRequest* Gecko_GetImageRequest(const nsStyleImage* image);
nsAtom* Gecko_GetImageElement(const nsStyleImage* image);
const nsStyleGradient* Gecko_GetGradientImageValue(const nsStyleImage* image);

// list-style-image style.
void Gecko_SetListStyleImageNone(nsStyleList* style_struct);

void Gecko_SetListStyleImageImageValue(nsStyleList* style_struct,
                                       mozilla::css::URLValue* aImageValue);

void Gecko_CopyListStyleImageFrom(nsStyleList* dest, const nsStyleList* src);

// cursor style.
void Gecko_SetCursorArrayLength(nsStyleUI* ui, size_t len);

void Gecko_SetCursorImageValue(nsCursorImage* aCursor,
                               mozilla::css::URLValue* aImageValue);

void Gecko_CopyCursorArrayFrom(nsStyleUI* dest, const nsStyleUI* src);

void Gecko_SetContentDataImageValue(nsStyleContentData* aList,
                                    mozilla::css::URLValue* aImageValue);

nsStyleContentData::CounterFunction* Gecko_SetCounterFunction(
    nsStyleContentData* content_data, mozilla::StyleContentType);

// Dirtiness tracking.
void Gecko_SetNodeFlags(const nsINode* node, uint32_t flags);
void Gecko_UnsetNodeFlags(const nsINode* node, uint32_t flags);
void Gecko_NoteDirtyElement(const mozilla::dom::Element*);
void Gecko_NoteDirtySubtreeForInvalidation(const mozilla::dom::Element*);
void Gecko_NoteAnimationOnlyDirtyElement(const mozilla::dom::Element*);

bool Gecko_AnimationNameMayBeReferencedFromStyle(const nsPresContext*,
                                                 nsAtom* name);

// Incremental restyle.
mozilla::PseudoStyleType Gecko_GetImplementedPseudo(
    const mozilla::dom::Element*);

// We'd like to return `nsChangeHint` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
uint32_t Gecko_CalcStyleDifference(const mozilla::ComputedStyle* old_style,
                                   const mozilla::ComputedStyle* new_style,
                                   bool* any_style_struct_changed,
                                   bool* reset_only_changed);

// Get an element snapshot for a given element from the table.
const mozilla::ServoElementSnapshot* Gecko_GetElementSnapshot(
    const mozilla::ServoElementSnapshotTable* table,
    const mozilla::dom::Element*);

// Have we seen this pointer before?
bool Gecko_HaveSeenPtr(mozilla::SeenPtrs* table, const void* ptr);

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

void Gecko_SetStyleGridTemplate(
    mozilla::UniquePtr<nsStyleGridTemplate>* grid_template,
    nsStyleGridTemplate* value);

nsStyleGridTemplate* Gecko_CreateStyleGridTemplate(uint32_t track_sizes,
                                                   uint32_t name_size);

void Gecko_CopyStyleGridTemplateValues(
    mozilla::UniquePtr<nsStyleGridTemplate>* grid_template,
    const nsStyleGridTemplate* other);

mozilla::css::GridTemplateAreasValue* Gecko_NewGridTemplateAreasValue(
    uint32_t areas, uint32_t templates, uint32_t columns);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::GridTemplateAreasValue,
                                   GridTemplateAreasValue);

// Clear the mContents, mCounterIncrements, mCounterResets, or mCounterSets
// field in nsStyleContent. This is needed to run the destructors, otherwise
// we'd leak the images, strings, and whatnot.
void Gecko_ClearAndResizeStyleContents(nsStyleContent* content,
                                       uint32_t how_many);

void Gecko_ClearAndResizeCounterIncrements(nsStyleContent* content,
                                           uint32_t how_many);

void Gecko_ClearAndResizeCounterResets(nsStyleContent* content,
                                       uint32_t how_many);

void Gecko_ClearAndResizeCounterSets(nsStyleContent* content,
                                     uint32_t how_many);

void Gecko_CopyStyleContentsFrom(nsStyleContent* content,
                                 const nsStyleContent* other);

void Gecko_CopyCounterResetsFrom(nsStyleContent* content,
                                 const nsStyleContent* other);

void Gecko_CopyCounterSetsFrom(nsStyleContent* content,
                               const nsStyleContent* other);

void Gecko_CopyCounterIncrementsFrom(nsStyleContent* content,
                                     const nsStyleContent* other);

void Gecko_EnsureImageLayersLength(nsStyleImageLayers* layers, size_t len,
                                   nsStyleImageLayers::LayerType layer_type);

void Gecko_EnsureStyleAnimationArrayLength(void* array, size_t len);
void Gecko_EnsureStyleTransitionArrayLength(void* array, size_t len);

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
    nsTArray<mozilla::Keyframe>* keyframes, float offset,
    const nsTimingFunction* timingFunction);

// As with Gecko_GetOrCreateKeyframeAtStart except that this method will search
// from the beginning of |keyframes| for a Keyframe with matching timing
// function and an offset of 0.0.
// Furthermore, if a matching Keyframe is not found, a new Keyframe will be
// inserted after the *last* Keyframe in |keyframes| with offset 0.0.
mozilla::Keyframe* Gecko_GetOrCreateInitialKeyframe(
    nsTArray<mozilla::Keyframe>* keyframes,
    const nsTimingFunction* timingFunction);

// As with Gecko_GetOrCreateKeyframeAtStart except that this method will search
// from the *end* of |keyframes| for a Keyframe with matching timing function
// and an offset of 1.0. If a matching Keyframe is not found, a new Keyframe
// will be appended to the end of |keyframes|.
mozilla::Keyframe* Gecko_GetOrCreateFinalKeyframe(
    nsTArray<mozilla::Keyframe>* keyframes,
    const nsTimingFunction* timingFunction);

// Appends and returns a new PropertyValuePair to |aProperties| initialized with
// its mProperty member set to |aProperty| and all other members initialized to
// their default values.
mozilla::PropertyValuePair* Gecko_AppendPropertyValuePair(
    nsTArray<mozilla::PropertyValuePair>*, nsCSSPropertyID aProperty);

// Clean up pointer-based coordinates
void Gecko_ResetStyleCoord(nsStyleUnit* unit, nsStyleUnion* value);

// Set an nsStyleCoord to a computed `calc()` value
void Gecko_SetStyleCoordCalcValue(nsStyleUnit* unit, nsStyleUnion* value,
                                  nsStyleCoord::CalcValue calc);

void Gecko_CopyShapeSourceFrom(mozilla::StyleShapeSource* dst,
                               const mozilla::StyleShapeSource* src);

void Gecko_DestroyShapeSource(mozilla::StyleShapeSource* shape);

void Gecko_NewShapeImage(mozilla::StyleShapeSource* shape);

void Gecko_StyleShapeSource_SetURLValue(mozilla::StyleShapeSource* shape,
                                        mozilla::css::URLValue* uri);

void Gecko_SetToSVGPath(
    mozilla::StyleShapeSource* shape,
    mozilla::StyleForgottenArcSlicePtr<mozilla::StylePathCommand>,
    mozilla::StyleFillRule);

void Gecko_SetStyleMotion(mozilla::UniquePtr<mozilla::StyleMotion>* aMotion,
                          mozilla::StyleMotion* aValue);

mozilla::StyleMotion* Gecko_NewStyleMotion();

void Gecko_CopyStyleMotions(mozilla::UniquePtr<mozilla::StyleMotion>* motion,
                            const mozilla::StyleMotion* other);

void Gecko_ResetFilters(nsStyleEffects* effects, size_t new_len);

void Gecko_CopyFiltersFrom(nsStyleEffects* aSrc, nsStyleEffects* aDest);

void Gecko_nsStyleFilter_SetURLValue(nsStyleFilter* effects,
                                     mozilla::css::URLValue* uri);

void Gecko_nsStyleSVGPaint_CopyFrom(nsStyleSVGPaint* dest,
                                    const nsStyleSVGPaint* src);

void Gecko_nsStyleSVGPaint_SetURLValue(nsStyleSVGPaint* paint,
                                       mozilla::css::URLValue* uri);

void Gecko_nsStyleSVGPaint_Reset(nsStyleSVGPaint* paint);

void Gecko_nsStyleSVG_SetDashArrayLength(nsStyleSVG* svg, uint32_t len);

void Gecko_nsStyleSVG_CopyDashArray(nsStyleSVG* dst, const nsStyleSVG* src);

void Gecko_nsStyleSVG_SetContextPropertiesLength(nsStyleSVG* svg, uint32_t len);

void Gecko_nsStyleSVG_CopyContextProperties(nsStyleSVG* dst,
                                            const nsStyleSVG* src);

mozilla::css::URLValue* Gecko_URLValue_Create(
    mozilla::StyleStrong<RawServoCssUrlData> url, mozilla::CORSMode aCORSMode);

size_t Gecko_URLValue_SizeOfIncludingThis(mozilla::css::URLValue* url);

void Gecko_GetComputedURLSpec(const mozilla::css::URLValue* url,
                              nsCString* spec);

void Gecko_GetComputedImageURLSpec(const mozilla::css::URLValue* url,
                                   nsCString* spec);

void Gecko_nsIURI_Debug(nsIURI*, nsCString* spec);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::URLValue, CSSURLValue);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::URLExtraData, URLExtraData);

void Gecko_FillAllImageLayers(nsStyleImageLayers* layers, uint32_t max_len);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

float Gecko_FontStretch_ToFloat(mozilla::FontStretch aStretch);

void Gecko_FontStretch_SetFloat(mozilla::FontStretch* aStretch,
                                float aFloatValue);

float Gecko_FontSlantStyle_ToFloat(mozilla::FontSlantStyle aStyle);
void Gecko_FontSlantStyle_SetNormal(mozilla::FontSlantStyle*);
void Gecko_FontSlantStyle_SetItalic(mozilla::FontSlantStyle*);

void Gecko_FontSlantStyle_SetOblique(mozilla::FontSlantStyle*,
                                     float angle_degrees);

void Gecko_FontSlantStyle_Get(mozilla::FontSlantStyle, bool* normal,
                              bool* italic, float* oblique_angle);

float Gecko_FontWeight_ToFloat(mozilla::FontWeight aWeight);

void Gecko_FontWeight_SetFloat(mozilla::FontWeight* aWeight, float aFloatValue);

void Gecko_nsStyleFont_SetLang(nsStyleFont* font, nsAtom* atom);

void Gecko_nsStyleFont_CopyLangFrom(nsStyleFont* aFont,
                                    const nsStyleFont* aSource);

// Moves the generic family in the font-family to the front, or prepends
// aDefaultGeneric, so that user-configured fonts take precedent over document
// fonts.
//
// Document fonts may still be used as fallback for unsupported glyphs though.
void Gecko_nsStyleFont_PrioritizeUserFonts(
    nsStyleFont* font, mozilla::StyleGenericFontFamily aDefaultGeneric);

nscoord Gecko_nsStyleFont_ComputeMinSize(const nsStyleFont*,
                                         const mozilla::dom::Document*);

// Computes the default generic font for a generic family and language.
mozilla::StyleGenericFontFamily Gecko_nsStyleFont_ComputeDefaultFontType(
    const mozilla::dom::Document*,
    mozilla::StyleGenericFontFamily generic_family, nsAtom* language);

mozilla::FontSizePrefs Gecko_GetBaseSize(nsAtom* lang);

// XBL related functions.
const mozilla::dom::Element* Gecko_GetBindingParent(
    const mozilla::dom::Element*);

struct GeckoFontMetrics {
  nscoord mChSize;  // -1.0 indicates not found
  nscoord mXSize;
};

GeckoFontMetrics Gecko_GetFontMetrics(const nsPresContext*, bool is_vertical,
                                      const nsStyleFont* font,
                                      nscoord font_size,
                                      bool use_user_font_set);

mozilla::StyleSheet* Gecko_StyleSheet_Clone(
    const mozilla::StyleSheet* aSheet,
    const mozilla::StyleSheet* aNewParentSheet);

void Gecko_StyleSheet_AddRef(const mozilla::StyleSheet* aSheet);
void Gecko_StyleSheet_Release(const mozilla::StyleSheet* aSheet);
nsCSSKeyword Gecko_LookupCSSKeyword(const uint8_t* string, uint32_t len);
const char* Gecko_CSSKeywordString(nsCSSKeyword keyword, uint32_t* len);
bool Gecko_IsDocumentBody(const mozilla::dom::Element* element);

// We use an int32_t here instead of a LookAndFeel::ColorID
// because forward-declaring a nested enum/struct is impossible
nscolor Gecko_GetLookAndFeelSystemColor(int32_t color_id,
                                        const mozilla::dom::Document*);

void Gecko_AddPropertyToSet(nsCSSPropertyIDSet*, nsCSSPropertyID);

// Style-struct management.
#define STYLE_STRUCT(name)                                                   \
  void Gecko_Construct_Default_nsStyle##name(nsStyle##name* ptr,             \
                                             const mozilla::dom::Document*); \
  void Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,                 \
                                         const nsStyle##name* other);        \
  void Gecko_Destroy_nsStyle##name(nsStyle##name* ptr);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

void Gecko_RegisterProfilerThread(const char* name);
void Gecko_UnregisterProfilerThread();

#ifdef MOZ_GECKO_PROFILER
void Gecko_Construct_AutoProfilerLabel(mozilla::AutoProfilerLabel*,
                                       JS::ProfilingCategoryPair);
void Gecko_Destroy_AutoProfilerLabel(mozilla::AutoProfilerLabel*);
#endif

bool Gecko_DocumentRule_UseForPresentation(
    const mozilla::dom::Document*, const nsACString* aPattern,
    mozilla::css::DocumentMatchingFunction);

// Allocator hinting.
void Gecko_SetJemallocThreadLocalArena(bool enabled);

// Pseudo-element flags.
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  const uint32_t SERVO_CSS_PSEUDO_ELEMENT_FLAGS_##name_ = flags_;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

bool Gecko_ErrorReportingEnabled(const mozilla::StyleSheet* sheet,
                                 const mozilla::css::Loader* loader);

void Gecko_ReportUnexpectedCSSError(
    const mozilla::StyleSheet* sheet, const mozilla::css::Loader* loader,
    nsIURI* uri, const char* message, const char* param, uint32_t paramLen,
    const char* prefix, const char* prefixParam, uint32_t prefixParamLen,
    const char* suffix, const char* source, uint32_t sourceLen,
    const char* selectors, uint32_t selectorsLen, uint32_t lineNumber,
    uint32_t colNumber);

// DOM APIs.
void Gecko_ContentList_AppendAll(nsSimpleContentList* aContentList,
                                 const mozilla::dom::Element** aElements,
                                 size_t aLength);

// FIXME(emilio): These two below should be a single function that takes a
// `const DocumentOrShadowRoot*`, but that doesn't make MSVC builds happy for a
// reason I haven't really dug into.
const nsTArray<mozilla::dom::Element*>* Gecko_Document_GetElementsWithId(
    const mozilla::dom::Document*, nsAtom* aId);

const nsTArray<mozilla::dom::Element*>* Gecko_ShadowRoot_GetElementsWithId(
    const mozilla::dom::ShadowRoot*, nsAtom* aId);

// Check the value of the given bool preference. The pref name needs to
// be null-terminated.
bool Gecko_GetBoolPrefValue(const char* pref_name);

// Returns true if we're currently performing the servo traversal.
bool Gecko_IsInServoTraversal();

// Returns true if we're currently on the main thread.
bool Gecko_IsMainThread();

// Media feature helpers.
//
// Defined in nsMediaFeatures.cpp.
mozilla::StyleDisplayMode Gecko_MediaFeatures_GetDisplayMode(
    const mozilla::dom::Document*);

uint32_t Gecko_MediaFeatures_GetColorDepth(const mozilla::dom::Document*);

void Gecko_MediaFeatures_GetDeviceSize(const mozilla::dom::Document*,
                                       nscoord* width, nscoord* height);

float Gecko_MediaFeatures_GetResolution(const mozilla::dom::Document*);
bool Gecko_MediaFeatures_PrefersReducedMotion(const mozilla::dom::Document*);
mozilla::StylePrefersColorScheme Gecko_MediaFeatures_PrefersColorScheme(
    const mozilla::dom::Document*);

mozilla::PointerCapabilities Gecko_MediaFeatures_PrimaryPointerCapabilities(
    const mozilla::dom::Document*);

mozilla::PointerCapabilities Gecko_MediaFeatures_AllPointerCapabilities(
    const mozilla::dom::Document*);

float Gecko_MediaFeatures_GetDevicePixelRatio(const mozilla::dom::Document*);

bool Gecko_MediaFeatures_HasSystemMetric(const mozilla::dom::Document*,
                                         nsAtom* metric,
                                         bool is_accessible_from_content);

bool Gecko_MediaFeatures_IsResourceDocument(const mozilla::dom::Document*);
nsAtom* Gecko_MediaFeatures_GetOperatingSystemVersion(
    const mozilla::dom::Document*);

}  // extern "C"

#endif  // mozilla_GeckoBindings_h
