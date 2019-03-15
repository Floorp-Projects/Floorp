/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Gecko to call into Servo */

#ifndef mozilla_ServoBindings_h
#define mozilla_ServoBindings_h

#include <stdint.h>

#include "mozilla/AtomArray.h"
#include "mozilla/css/SheetLoadData.h"
#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/ServoBindingTypes.h"
#include "nsChangeHint.h"
#include "nsColor.h"
#include "nsCSSValue.h"

class gfxFontFeatureValueSet;
class nsAtom;
class nsSimpleContentList;
struct gfxFontFeature;

namespace mozilla {
class SeenPtrs;
class ServoElementSnapshotTable;
class SharedFontList;
class StyleSheet;
enum class PseudoStyleType : uint8_t;
enum class OriginFlags : uint8_t;
struct Keyframe;

namespace css {
class LoaderReusableStyleSheets;
}

namespace gfx {
struct FontVariation;
}

namespace dom {
enum class IterationCompositeOperation : uint8_t;
}
}  // namespace mozilla

namespace nsStyleTransformMatrix {
enum class MatrixTransformOperator : uint8_t;
}

extern "C" {

// Element data
void Servo_Element_ClearData(RawGeckoElementBorrowed node);

size_t Servo_Element_SizeOfExcludingThisAndCVs(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    mozilla::SeenPtrs* seen_ptrs, RawGeckoElementBorrowed node);

bool Servo_Element_HasPrimaryComputedValues(RawGeckoElementBorrowed node);

ComputedStyleStrong Servo_Element_GetPrimaryComputedValues(
    RawGeckoElementBorrowed node);

bool Servo_Element_HasPseudoComputedValues(RawGeckoElementBorrowed node,
                                           size_t index);

ComputedStyleStrong Servo_Element_GetPseudoComputedValues(
    RawGeckoElementBorrowed node, size_t index);

bool Servo_Element_IsDisplayNone(RawGeckoElementBorrowed element);
bool Servo_Element_IsDisplayContents(RawGeckoElementBorrowed element);

bool Servo_Element_IsPrimaryStyleReusedViaRuleNode(
    RawGeckoElementBorrowed element);

void Servo_InvalidateStyleForDocStateChanges(
    RawGeckoElementBorrowed root, RawServoStyleSetBorrowed doc_styles,
    const nsTArray<RawServoAuthorStylesBorrowed>* non_document_styles,
    uint64_t aStatesChanged);

// Styleset and Stylesheet management

RawServoStyleSheetContentsStrong Servo_StyleSheet_FromUTF8Bytes(
    mozilla::css::Loader* loader, mozilla::StyleSheet* gecko_stylesheet,
    mozilla::css::SheetLoadData* load_data, const nsACString* bytes,
    mozilla::css::SheetParsingMode parsing_mode,
    RawGeckoURLExtraData* extra_data, uint32_t line_number_offset,
    nsCompatibility quirks_mode,
    mozilla::css::LoaderReusableStyleSheets* reusable_sheets,
    StyleUseCountersBorrowedOrNull use_counters);

void Servo_StyleSheet_FromUTF8BytesAsync(
    mozilla::css::SheetLoadDataHolder* load_data,
    RawGeckoURLExtraData* extra_data, const nsACString* bytes,
    mozilla::css::SheetParsingMode parsing_mode, uint32_t line_number_offset,
    nsCompatibility quirks_mode, bool should_record_use_counters);

RawServoStyleSheetContentsStrong Servo_StyleSheet_Empty(
    mozilla::css::SheetParsingMode parsing_mode);

bool Servo_StyleSheet_HasRules(RawServoStyleSheetContentsBorrowed sheet);

ServoCssRulesStrong Servo_StyleSheet_GetRules(
    RawServoStyleSheetContentsBorrowed sheet);

RawServoStyleSheetContentsStrong Servo_StyleSheet_Clone(
    RawServoStyleSheetContentsBorrowed sheet,
    const mozilla::StyleSheet* reference_sheet);

size_t Servo_StyleSheet_SizeOfIncludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    RawServoStyleSheetContentsBorrowed sheet);

void Servo_StyleSheet_GetSourceMapURL(RawServoStyleSheetContentsBorrowed sheet,
                                      nsAString* result);

void Servo_StyleSheet_GetSourceURL(RawServoStyleSheetContentsBorrowed sheet,
                                   nsAString* result);

// We'd like to return `OriginFlags` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
uint8_t Servo_StyleSheet_GetOrigin(RawServoStyleSheetContentsBorrowed sheet);

RawServoStyleSet* Servo_StyleSet_Init(RawGeckoPresContextBorrowed pres_context);
void Servo_StyleSet_RebuildCachedData(RawServoStyleSetBorrowed set);

// We'd like to return `OriginFlags` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
mozilla::MediumFeaturesChangedResult Servo_StyleSet_MediumFeaturesChanged(
    RawServoStyleSetBorrowed document_set,
    nsTArray<RawServoAuthorStylesBorrowedMut>* non_document_sets,
    bool may_affect_default_style);

void Servo_StyleSet_Drop(RawServoStyleSetOwned set);
void Servo_StyleSet_CompatModeChanged(RawServoStyleSetBorrowed raw_data);

void Servo_StyleSet_AppendStyleSheet(RawServoStyleSetBorrowed set,
                                     const mozilla::StyleSheet* gecko_sheet);

void Servo_StyleSet_RemoveStyleSheet(RawServoStyleSetBorrowed set,
                                     const mozilla::StyleSheet* gecko_sheet);

void Servo_StyleSet_InsertStyleSheetBefore(
    RawServoStyleSetBorrowed set, const mozilla::StyleSheet* gecko_sheet,
    const mozilla::StyleSheet* before);

void Servo_StyleSet_FlushStyleSheets(
    RawServoStyleSetBorrowed set, RawGeckoElementBorrowedOrNull doc_elem,
    const mozilla::ServoElementSnapshotTable* snapshots);

void Servo_StyleSet_SetAuthorStyleDisabled(RawServoStyleSetBorrowed set,
                                           bool author_style_disabled);

void Servo_StyleSet_NoteStyleSheetsChanged(
    RawServoStyleSetBorrowed set, mozilla::OriginFlags changed_origins);

bool Servo_StyleSet_GetKeyframesForName(
    RawServoStyleSetBorrowed set, RawGeckoElementBorrowed element,
    ComputedStyleBorrowed style, nsAtom* name,
    nsTimingFunctionBorrowed timing_function,
    RawGeckoKeyframeListBorrowedMut keyframe_list);

void Servo_StyleSet_GetFontFaceRules(RawServoStyleSetBorrowed set,
                                     RawGeckoFontFaceRuleListBorrowedMut list);

const RawServoCounterStyleRule* Servo_StyleSet_GetCounterStyleRule(
    RawServoStyleSetBorrowed set, nsAtom* name);

// This function may return nullptr or gfxFontFeatureValueSet with zero
// references.
gfxFontFeatureValueSet* Servo_StyleSet_BuildFontFeatureValueSet(
    RawServoStyleSetBorrowed set);

ComputedStyleStrong Servo_StyleSet_ResolveForDeclarations(
    RawServoStyleSetBorrowed set, ComputedStyleBorrowedOrNull parent_style,
    RawServoDeclarationBlockBorrowed declarations);

void Servo_SelectorList_Drop(RawServoSelectorListOwned selector_list);
RawServoSelectorList* Servo_SelectorList_Parse(const nsACString* selector_list);
RawServoSourceSizeList* Servo_SourceSizeList_Parse(const nsACString* value);

int32_t Servo_SourceSizeList_Evaluate(RawServoStyleSetBorrowed set,
                                      RawServoSourceSizeListBorrowedOrNull);

void Servo_SourceSizeList_Drop(RawServoSourceSizeListOwned);

bool Servo_SelectorList_Matches(RawGeckoElementBorrowed,
                                RawServoSelectorListBorrowed);

const RawGeckoElement* Servo_SelectorList_Closest(RawGeckoElementBorrowed,
                                                  RawServoSelectorListBorrowed);

const RawGeckoElement* Servo_SelectorList_QueryFirst(
    RawGeckoNodeBorrowed, RawServoSelectorListBorrowed,
    bool may_use_invalidation);

void Servo_SelectorList_QueryAll(RawGeckoNodeBorrowed,
                                 RawServoSelectorListBorrowed,
                                 nsSimpleContentList* content_list,
                                 bool may_use_invalidation);

void Servo_StyleSet_AddSizeOfExcludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    mozilla::ServoStyleSetSizes* sizes, RawServoStyleSetBorrowed set);

void Servo_UACache_AddSizeOf(mozilla::MallocSizeOf malloc_size_of,
                             mozilla::MallocSizeOf malloc_enclosing_size_of,
                             mozilla::ServoStyleSetSizes* sizes);

// AuthorStyles

RawServoAuthorStyles* Servo_AuthorStyles_Create();
void Servo_AuthorStyles_Drop(RawServoAuthorStylesOwned self);

// TODO(emilio): These will need to take a master style set to implement
// invalidation for Shadow DOM.
void Servo_AuthorStyles_AppendStyleSheet(
    RawServoAuthorStylesBorrowedMut self,
    const mozilla::StyleSheet* gecko_sheet);

void Servo_AuthorStyles_RemoveStyleSheet(
    RawServoAuthorStylesBorrowedMut self,
    const mozilla::StyleSheet* gecko_sheet);

void Servo_AuthorStyles_InsertStyleSheetBefore(
    RawServoAuthorStylesBorrowedMut self,
    const mozilla::StyleSheet* gecko_sheet, const mozilla::StyleSheet* before);

void Servo_AuthorStyles_ForceDirty(RawServoAuthorStylesBorrowedMut self);

// TODO(emilio): This will need to take an element and a master style set to
// implement invalidation for Shadow DOM.
void Servo_AuthorStyles_Flush(RawServoAuthorStylesBorrowedMut self,
                              RawServoStyleSetBorrowed document_styles);

size_t Servo_AuthorStyles_SizeOfIncludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    RawServoAuthorStylesBorrowed self);

void Servo_ComputedStyle_AddRef(ComputedStyleBorrowed ctx);

void Servo_ComputedStyle_Release(ComputedStyleBorrowed ctx);

bool Servo_StyleSet_MightHaveAttributeDependency(
    RawServoStyleSetBorrowed set, RawGeckoElementBorrowed element,
    nsAtom* local_name);

bool Servo_StyleSet_HasStateDependency(RawServoStyleSetBorrowed set,
                                       RawGeckoElementBorrowed element,
                                       uint64_t state);

bool Servo_StyleSet_HasDocumentStateDependency(RawServoStyleSetBorrowed set,
                                               uint64_t state);

// CSSRuleList

void Servo_CssRules_ListTypes(ServoCssRulesBorrowed rules,
                              nsTArrayBorrowed_uintptr_t result);

nsresult Servo_CssRules_InsertRule(ServoCssRulesBorrowed rules,
                                   RawServoStyleSheetContentsBorrowed sheet,
                                   const nsACString* rule, uint32_t index,
                                   bool nested, mozilla::css::Loader* loader,
                                   mozilla::StyleSheet* gecko_stylesheet,
                                   uint16_t* rule_type);

nsresult Servo_CssRules_DeleteRule(ServoCssRulesBorrowed rules, uint32_t index);

// CSS Rules

#define BASIC_RULE_FUNCS_WITHOUT_GETTER(type_)                    \
  void Servo_##type_##_Debug(RawServo##type_##Borrowed rule,      \
                             nsACString* result);                 \
  void Servo_##type_##_GetCssText(RawServo##type_##Borrowed rule, \
                                  nsAString* result);

#define BASIC_RULE_FUNCS(type_)                                    \
  RawServo##type_##RuleStrong Servo_CssRules_Get##type_##RuleAt(   \
      ServoCssRulesBorrowed rules, uint32_t index, uint32_t* line, \
      uint32_t* column);                                           \
  BASIC_RULE_FUNCS_WITHOUT_GETTER(type_##Rule)

#define GROUP_RULE_FUNCS(type_)                     \
  BASIC_RULE_FUNCS(type_)                           \
  ServoCssRulesStrong Servo_##type_##Rule_GetRules( \
      RawServo##type_##RuleBorrowed rule);

BASIC_RULE_FUNCS(Style)
BASIC_RULE_FUNCS(Import)
BASIC_RULE_FUNCS_WITHOUT_GETTER(Keyframe)
BASIC_RULE_FUNCS(Keyframes)
GROUP_RULE_FUNCS(Media)
GROUP_RULE_FUNCS(MozDocument)
BASIC_RULE_FUNCS(Namespace)
BASIC_RULE_FUNCS(Page)
GROUP_RULE_FUNCS(Supports)
BASIC_RULE_FUNCS(FontFeatureValues)
BASIC_RULE_FUNCS(FontFace)
BASIC_RULE_FUNCS(CounterStyle)

#undef GROUP_RULE_FUNCS
#undef BASIC_RULE_FUNCS
#undef BASIC_RULE_FUNCS_WITHOUT_GETTER

RawServoDeclarationBlockStrong Servo_StyleRule_GetStyle(
    RawServoStyleRuleBorrowed rule);

void Servo_StyleRule_SetStyle(RawServoStyleRuleBorrowed rule,
                              RawServoDeclarationBlockBorrowed declarations);

void Servo_StyleRule_GetSelectorText(RawServoStyleRuleBorrowed rule,
                                     nsAString* result);

void Servo_StyleRule_GetSelectorTextAtIndex(RawServoStyleRuleBorrowed rule,
                                            uint32_t index, nsAString* result);

void Servo_StyleRule_GetSpecificityAtIndex(RawServoStyleRuleBorrowed rule,
                                           uint32_t index,
                                           uint64_t* specificity);

void Servo_StyleRule_GetSelectorCount(RawServoStyleRuleBorrowed rule,
                                      uint32_t* count);

bool Servo_StyleRule_SelectorMatchesElement(
    RawServoStyleRuleBorrowed, RawGeckoElementBorrowed, uint32_t index,
    mozilla::PseudoStyleType pseudo_type);

bool Servo_StyleRule_SetSelectorText(RawServoStyleSheetContentsBorrowed sheet,
                                     RawServoStyleRuleBorrowed rule,
                                     const nsAString* text);

void Servo_ImportRule_GetHref(RawServoImportRuleBorrowed rule,
                              nsAString* result);

const mozilla::StyleSheet* Servo_ImportRule_GetSheet(
    RawServoImportRuleBorrowed rule);

void Servo_ImportRule_SetSheet(RawServoImportRuleBorrowed rule,
                               mozilla::StyleSheet* sheet);

void Servo_Keyframe_GetKeyText(RawServoKeyframeBorrowed keyframe,
                               nsAString* result);

// Returns whether it successfully changes the key text.
bool Servo_Keyframe_SetKeyText(RawServoKeyframeBorrowed keyframe,
                               const nsACString* text);

RawServoDeclarationBlockStrong Servo_Keyframe_GetStyle(
    RawServoKeyframeBorrowed keyframe);

void Servo_Keyframe_SetStyle(RawServoKeyframeBorrowed keyframe,
                             RawServoDeclarationBlockBorrowed declarations);

nsAtom* Servo_KeyframesRule_GetName(RawServoKeyframesRuleBorrowed rule);

// This method takes an addrefed nsAtom.
void Servo_KeyframesRule_SetName(RawServoKeyframesRuleBorrowed rule,
                                 nsAtom* name);

uint32_t Servo_KeyframesRule_GetCount(RawServoKeyframesRuleBorrowed rule);

RawServoKeyframeStrong Servo_KeyframesRule_GetKeyframeAt(
    RawServoKeyframesRuleBorrowed rule, uint32_t index, uint32_t* line,
    uint32_t* column);

// Returns the index of the rule, max value of uint32_t if nothing found.
uint32_t Servo_KeyframesRule_FindRule(RawServoKeyframesRuleBorrowed rule,
                                      const nsACString* key);

// Returns whether it successfully appends the rule.
bool Servo_KeyframesRule_AppendRule(RawServoKeyframesRuleBorrowed rule,
                                    RawServoStyleSheetContentsBorrowed sheet,
                                    const nsACString* css);

void Servo_KeyframesRule_DeleteRule(RawServoKeyframesRuleBorrowed rule,
                                    uint32_t index);

RawServoMediaListStrong Servo_MediaRule_GetMedia(
    RawServoMediaRuleBorrowed rule);

nsAtom* Servo_NamespaceRule_GetPrefix(RawServoNamespaceRuleBorrowed rule);
nsAtom* Servo_NamespaceRule_GetURI(RawServoNamespaceRuleBorrowed rule);

RawServoDeclarationBlockStrong Servo_PageRule_GetStyle(
    RawServoPageRuleBorrowed rule);

void Servo_PageRule_SetStyle(RawServoPageRuleBorrowed rule,
                             RawServoDeclarationBlockBorrowed declarations);

void Servo_SupportsRule_GetConditionText(RawServoSupportsRuleBorrowed rule,
                                         nsAString* result);

void Servo_MozDocumentRule_GetConditionText(
    RawServoMozDocumentRuleBorrowed rule, nsAString* result);

void Servo_FontFeatureValuesRule_GetFontFamily(
    RawServoFontFeatureValuesRuleBorrowed rule, nsAString* result);

void Servo_FontFeatureValuesRule_GetValueText(
    RawServoFontFeatureValuesRuleBorrowed rule, nsAString* result);

RawServoFontFaceRuleStrong Servo_FontFaceRule_CreateEmpty();

RawServoFontFaceRuleStrong Servo_FontFaceRule_Clone(
    RawServoFontFaceRuleBorrowed rule);

void Servo_FontFaceRule_GetSourceLocation(RawServoFontFaceRuleBorrowed rule,
                                          uint32_t* line, uint32_t* column);

uint32_t Servo_FontFaceRule_Length(RawServoFontFaceRuleBorrowed rule);

nsCSSFontDesc Servo_FontFaceRule_IndexGetter(RawServoFontFaceRuleBorrowed rule,
                                             uint32_t index);

void Servo_FontFaceRule_GetDeclCssText(RawServoFontFaceRuleBorrowed rule,
                                       nsAString* result);

bool Servo_FontFaceRule_GetFontWeight(
    RawServoFontFaceRuleBorrowed rule,
    mozilla::StyleComputedFontWeightRange* out);

bool Servo_FontFaceRule_GetFontDisplay(RawServoFontFaceRuleBorrowed rule,
                                       mozilla::StyleFontDisplay* out);

bool Servo_FontFaceRule_GetFontStyle(
    RawServoFontFaceRuleBorrowed rule,
    mozilla::StyleComputedFontStyleDescriptor* out);

bool Servo_FontFaceRule_GetFontStretch(
    RawServoFontFaceRuleBorrowed rule,
    mozilla::StyleComputedFontStretchRange* out);

bool Servo_FontFaceRule_GetFontLanguageOverride(
    RawServoFontFaceRuleBorrowed rule, mozilla::StyleFontLanguageOverride* out);

nsAtom* Servo_FontFaceRule_GetFamilyName(RawServoFontFaceRuleBorrowed rule);

const mozilla::StyleUnicodeRange* Servo_FontFaceRule_GetUnicodeRanges(
    RawServoFontFaceRuleBorrowed rule, size_t* out_len);

void Servo_FontFaceRule_GetSources(
    RawServoFontFaceRuleBorrowed rule,
    nsTArray<mozilla::StyleFontFaceSourceListComponent>* components);

void Servo_FontFaceRule_GetVariationSettings(
    RawServoFontFaceRuleBorrowed rule,
    nsTArray<mozilla::gfx::FontVariation>* out);

void Servo_FontFaceRule_GetFeatureSettings(RawServoFontFaceRuleBorrowed rule,
                                           nsTArray<gfxFontFeature>* out);

void Servo_FontFaceRule_GetDescriptorCssText(RawServoFontFaceRuleBorrowed rule,
                                             nsCSSFontDesc desc,
                                             nsAString* result);

bool Servo_FontFaceRule_SetDescriptor(RawServoFontFaceRuleBorrowed rule,
                                      nsCSSFontDesc desc,
                                      const nsACString* value,
                                      RawGeckoURLExtraData* data);

void Servo_FontFaceRule_ResetDescriptor(RawServoFontFaceRuleBorrowed rule,
                                        nsCSSFontDesc desc);

nsAtom* Servo_CounterStyleRule_GetName(RawServoCounterStyleRuleBorrowed rule);

bool Servo_CounterStyleRule_SetName(RawServoCounterStyleRuleBorrowed rule,
                                    const nsACString* name);

uint32_t Servo_CounterStyleRule_GetGeneration(
    RawServoCounterStyleRuleBorrowed rule);

uint8_t Servo_CounterStyleRule_GetSystem(RawServoCounterStyleRuleBorrowed rule);

nsAtom* Servo_CounterStyleRule_GetExtended(
    RawServoCounterStyleRuleBorrowed rule);

int32_t Servo_CounterStyleRule_GetFixedFirstValue(
    RawServoCounterStyleRuleBorrowed rule);

nsAtom* Servo_CounterStyleRule_GetFallback(
    RawServoCounterStyleRuleBorrowed rule);

void Servo_CounterStyleRule_GetDescriptor(RawServoCounterStyleRuleBorrowed rule,
                                          nsCSSCounterDesc desc,
                                          nsCSSValueBorrowedMut result);

void Servo_CounterStyleRule_GetDescriptorCssText(
    RawServoCounterStyleRuleBorrowed rule, nsCSSCounterDesc desc,
    nsAString* result);

bool Servo_CounterStyleRule_SetDescriptor(RawServoCounterStyleRuleBorrowed rule,
                                          nsCSSCounterDesc desc,
                                          const nsACString* value);

// Animations API

RawServoDeclarationBlockStrong Servo_ParseProperty(
    nsCSSPropertyID property, const nsACString* value,
    RawGeckoURLExtraData* data, mozilla::ParsingMode parsing_mode,
    nsCompatibility quirks_mode, mozilla::css::Loader* loader);

bool Servo_ParseEasing(const nsAString* easing, RawGeckoURLExtraData* data,
                       nsTimingFunctionBorrowedMut output);

void Servo_SerializeEasing(nsTimingFunctionBorrowed easing, nsAString* output);

void Servo_SerializeBorderRadius(const mozilla::StyleBorderRadius*, nsAString*);

void Servo_GetComputedKeyframeValues(
    RawGeckoKeyframeListBorrowed keyframes, RawGeckoElementBorrowed element,
    ComputedStyleBorrowed style, RawServoStyleSetBorrowed set,
    RawGeckoComputedKeyframeValuesListBorrowedMut result);

RawServoAnimationValueStrong Servo_ComputedValues_ExtractAnimationValue(
    ComputedStyleBorrowed computed_values, nsCSSPropertyID property);

bool Servo_ComputedValues_SpecifiesAnimationsOrTransitions(
    ComputedStyleBorrowed computed_values);

bool Servo_Property_IsAnimatable(nsCSSPropertyID property);
bool Servo_Property_IsTransitionable(nsCSSPropertyID property);
bool Servo_Property_IsDiscreteAnimatable(nsCSSPropertyID property);

void Servo_GetProperties_Overriding_Animation(RawGeckoElementBorrowed,
                                              RawGeckoCSSPropertyIDListBorrowed,
                                              nsCSSPropertyIDSetBorrowedMut);

void Servo_MatrixTransform_Operate(
    nsStyleTransformMatrix::MatrixTransformOperator matrix_operator,
    const RawGeckoGfxMatrix4x4* from, const RawGeckoGfxMatrix4x4* to,
    double progress, RawGeckoGfxMatrix4x4* result);

void Servo_GetAnimationValues(
    RawServoDeclarationBlockBorrowed declarations,
    RawGeckoElementBorrowed element, ComputedStyleBorrowed style,
    RawServoStyleSetBorrowed style_set,
    RawGeckoServoAnimationValueListBorrowedMut animation_values);

// AnimationValues handling

RawServoAnimationValueStrong Servo_AnimationValues_Interpolate(
    RawServoAnimationValueBorrowed from, RawServoAnimationValueBorrowed to,
    double progress);

bool Servo_AnimationValues_IsInterpolable(RawServoAnimationValueBorrowed from,
                                          RawServoAnimationValueBorrowed to);

RawServoAnimationValueStrong Servo_AnimationValues_Add(
    RawServoAnimationValueBorrowed a, RawServoAnimationValueBorrowed b);

RawServoAnimationValueStrong Servo_AnimationValues_Accumulate(
    RawServoAnimationValueBorrowed a, RawServoAnimationValueBorrowed b,
    uint64_t count);

RawServoAnimationValueStrong Servo_AnimationValues_GetZeroValue(
    RawServoAnimationValueBorrowed value_to_match);

double Servo_AnimationValues_ComputeDistance(
    RawServoAnimationValueBorrowed from, RawServoAnimationValueBorrowed to);

void Servo_AnimationValue_Serialize(RawServoAnimationValueBorrowed value,
                                    nsCSSPropertyID property,
                                    nsAString* buffer);

nscolor Servo_AnimationValue_GetColor(RawServoAnimationValueBorrowed value,
                                      nscolor foregroundColor);
RawServoAnimationValueStrong Servo_AnimationValue_Color(nsCSSPropertyID,
                                                        nscolor);

float Servo_AnimationValue_GetOpacity(RawServoAnimationValueBorrowed value);
RawServoAnimationValueStrong Servo_AnimationValue_Opacity(float);

void Servo_AnimationValue_GetTransform(RawServoAnimationValueBorrowed value,
                                       RefPtr<nsCSSValueSharedList>* list);

RawServoAnimationValueStrong Servo_AnimationValue_Transform(
    const nsCSSValueSharedList& list);

bool Servo_AnimationValue_DeepEqual(RawServoAnimationValueBorrowed,
                                    RawServoAnimationValueBorrowed);

RawServoDeclarationBlockStrong Servo_AnimationValue_Uncompute(
    RawServoAnimationValueBorrowed value);

RawServoAnimationValueStrong Servo_AnimationValue_Compute(
    RawGeckoElementBorrowed element,
    RawServoDeclarationBlockBorrowed declarations, ComputedStyleBorrowed style,
    RawServoStyleSetBorrowed raw_data);

// Style attribute

RawServoDeclarationBlockStrong Servo_ParseStyleAttribute(
    const nsACString* data, RawGeckoURLExtraData* extra_data,
    nsCompatibility quirks_mode, mozilla::css::Loader* loader);

RawServoDeclarationBlockStrong Servo_DeclarationBlock_CreateEmpty();

RawServoDeclarationBlockStrong Servo_DeclarationBlock_Clone(
    RawServoDeclarationBlockBorrowed declarations);

bool Servo_DeclarationBlock_Equals(RawServoDeclarationBlockBorrowed a,
                                   RawServoDeclarationBlockBorrowed b);

void Servo_DeclarationBlock_GetCssText(
    RawServoDeclarationBlockBorrowed declarations, nsAString* result);

void Servo_DeclarationBlock_SerializeOneValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    nsAString* buffer, ComputedStyleBorrowedOrNull computed_values,
    RawServoDeclarationBlockBorrowedOrNull custom_properties);

uint32_t Servo_DeclarationBlock_Count(
    RawServoDeclarationBlockBorrowed declarations);

bool Servo_DeclarationBlock_GetNthProperty(
    RawServoDeclarationBlockBorrowed declarations, uint32_t index,
    nsAString* result);

void Servo_DeclarationBlock_GetPropertyValue(
    RawServoDeclarationBlockBorrowed declarations, const nsACString* property,
    nsAString* value);

void Servo_DeclarationBlock_GetPropertyValueById(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    nsAString* value);

bool Servo_DeclarationBlock_GetPropertyIsImportant(
    RawServoDeclarationBlockBorrowed declarations, const nsACString* property);

bool Servo_DeclarationBlock_SetProperty(
    RawServoDeclarationBlockBorrowed declarations, const nsACString* property,
    const nsACString* value, bool is_important, RawGeckoURLExtraData* data,
    mozilla::ParsingMode parsing_mode, nsCompatibility quirks_mode,
    mozilla::css::Loader* loader, mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_SetPropertyToAnimationValue(
    RawServoDeclarationBlockBorrowed declarations,
    RawServoAnimationValueBorrowed animation_value);

bool Servo_DeclarationBlock_SetPropertyById(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    const nsACString* value, bool is_important, RawGeckoURLExtraData* data,
    mozilla::ParsingMode parsing_mode, nsCompatibility quirks_mode,
    mozilla::css::Loader* loader, mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_RemoveProperty(
    RawServoDeclarationBlockBorrowed declarations, const nsACString* property,
    mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_RemovePropertyById(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_HasCSSWideKeyword(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property);

// Compose animation value for a given property.
// |base_values| is nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>.
// We use RawServoAnimationValueTableBorrowed to avoid exposing
// nsRefPtrHashtable in FFI.
void Servo_AnimationCompose(
    RawServoAnimationValueMapBorrowedMut animation_values,
    RawServoAnimationValueTableBorrowed base_values, nsCSSPropertyID property,
    RawGeckoAnimationPropertySegmentBorrowed animation_segment,
    RawGeckoAnimationPropertySegmentBorrowed last_segment,
    RawGeckoComputedTimingBorrowed computed_timing,
    mozilla::dom::IterationCompositeOperation iter_composite);

// Calculate the result of interpolating given animation segment at the given
// progress and current iteration.
// This includes combining the segment endpoints with the underlying value
// and/or last value depending the composite modes specified on the
// segment endpoints and the supplied iteration composite mode.
// The caller is responsible for providing an underlying value and
// last value in all situations where there are needed.
RawServoAnimationValueStrong Servo_ComposeAnimationSegment(
    RawGeckoAnimationPropertySegmentBorrowed animation_segment,
    RawServoAnimationValueBorrowedOrNull underlying_value,
    RawServoAnimationValueBorrowedOrNull last_value,
    mozilla::dom::IterationCompositeOperation iter_composite, double progress,
    uint64_t current_iteration);

// presentation attributes

bool Servo_DeclarationBlock_PropertyIsSet(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property);

void Servo_DeclarationBlock_SetIdentStringValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    nsAtom* value);

void Servo_DeclarationBlock_SetKeywordValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    int32_t value);

void Servo_DeclarationBlock_SetIntValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    int32_t value);

void Servo_DeclarationBlock_SetPixelValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    float value);

void Servo_DeclarationBlock_SetLengthValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    float value, nsCSSUnit unit);

void Servo_DeclarationBlock_SetNumberValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    float value);

void Servo_DeclarationBlock_SetPercentValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    float value);

void Servo_DeclarationBlock_SetAutoValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property);

void Servo_DeclarationBlock_SetCurrentColor(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property);

void Servo_DeclarationBlock_SetColorValue(
    RawServoDeclarationBlockBorrowed declarations, nsCSSPropertyID property,
    nscolor value);

void Servo_DeclarationBlock_SetFontFamily(
    RawServoDeclarationBlockBorrowed declarations, const nsAString& value);

void Servo_DeclarationBlock_SetTextDecorationColorOverride(
    RawServoDeclarationBlockBorrowed declarations);

void Servo_DeclarationBlock_SetBackgroundImage(
    RawServoDeclarationBlockBorrowed declarations, const nsAString& value,
    RawGeckoURLExtraData* extra_data);

// MediaList

RawServoMediaListStrong Servo_MediaList_Create();

RawServoMediaListStrong Servo_MediaList_DeepClone(
    RawServoMediaListBorrowed list);

bool Servo_MediaList_Matches(RawServoMediaListBorrowed list,
                             RawServoStyleSetBorrowed set);

void Servo_MediaList_GetText(RawServoMediaListBorrowed list, nsAString* result);

void Servo_MediaList_SetText(RawServoMediaListBorrowed list,
                             const nsACString* text,
                             mozilla::dom::CallerType aCallerType);

uint32_t Servo_MediaList_GetLength(RawServoMediaListBorrowed list);

bool Servo_MediaList_GetMediumAt(RawServoMediaListBorrowed list, uint32_t index,
                                 nsAString* result);

void Servo_MediaList_AppendMedium(RawServoMediaListBorrowed list,
                                  const nsACString* new_medium);

bool Servo_MediaList_DeleteMedium(RawServoMediaListBorrowed list,
                                  const nsACString* old_medium);

size_t Servo_MediaList_SizeOfIncludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    RawServoMediaListBorrowed list);

// CSS supports();

bool Servo_CSSSupports2(const nsACString* name, const nsACString* value);
bool Servo_CSSSupports(const nsACString* cond);

// Computed style data

ComputedStyleStrong Servo_ComputedValues_GetForAnonymousBox(
    ComputedStyleBorrowedOrNull parent_style_or_null, mozilla::PseudoStyleType,
    RawServoStyleSetBorrowed set);

ComputedStyleStrong Servo_ComputedValues_Inherit(
    RawServoStyleSetBorrowed, mozilla::PseudoStyleType,
    ComputedStyleBorrowedOrNull parent_style, mozilla::InheritTarget);

uint8_t Servo_ComputedValues_GetStyleBits(ComputedStyleBorrowed values);

bool Servo_ComputedValues_EqualCustomProperties(
    ServoComputedDataBorrowed first, ServoComputedDataBorrowed second);

// Gets the source style rules for the computed values. This returns
// the result via rules, which would include a list of unowned pointers
// to RawServoStyleRule.
void Servo_ComputedValues_GetStyleRuleList(
    ComputedStyleBorrowed values, RawGeckoServoStyleRuleListBorrowedMut rules);

// Initialize Servo components. Should be called exactly once at startup.
void Servo_Initialize(RawGeckoURLExtraData* dummy_url_data);

// Initialize Servo on a cooperative Quantum DOM thread.
void Servo_InitializeCooperativeThread();

// Shut down Servo components. Should be called exactly once at shutdown.
void Servo_Shutdown();

// Restyle and change hints.
void Servo_NoteExplicitHints(RawGeckoElementBorrowed, mozilla::RestyleHint,
                             nsChangeHint);

// We'd like to return `nsChangeHint` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
uint32_t Servo_TakeChangeHint(RawGeckoElementBorrowed element,
                              bool* was_restyled);

ComputedStyleStrong Servo_ResolveStyle(RawGeckoElementBorrowed element,
                                       RawServoStyleSetBorrowed set);

ComputedStyleStrong Servo_ResolvePseudoStyle(
    RawGeckoElementBorrowed element, mozilla::PseudoStyleType pseudo_type,
    bool is_probe, ComputedStyleBorrowedOrNull inherited_style,
    RawServoStyleSetBorrowed set);

ComputedStyleStrong Servo_ComputedValues_ResolveXULTreePseudoStyle(
    RawGeckoElementBorrowed element, nsAtom* pseudo_tag,
    ComputedStyleBorrowed inherited_style, const mozilla::AtomArray* input_word,
    RawServoStyleSetBorrowed set);

void Servo_SetExplicitStyle(RawGeckoElementBorrowed element,
                            ComputedStyleBorrowed primary_style);

bool Servo_HasAuthorSpecifiedRules(RawServoStyleSetBorrowed set,
                                   ComputedStyleBorrowed style,
                                   RawGeckoElementBorrowed element,
                                   uint32_t rule_type_mask);

// Resolves style for an element or pseudo-element without processing pending
// restyles first. The Element and its ancestors may be unstyled, have pending
// restyles, or be in a display:none subtree. Styles are cached when possible,
// though caching is not possible within display:none subtrees, and the styles
// may be invalidated by already-scheduled restyles.
//
// The tree must be in a consistent state such that a normal traversal could be
// performed, and this function maintains that invariant.

ComputedStyleStrong Servo_ResolveStyleLazily(
    RawGeckoElementBorrowed element, mozilla::PseudoStyleType pseudo_type,
    mozilla::StyleRuleInclusion rule_inclusion,
    const mozilla::ServoElementSnapshotTable* snapshots,
    RawServoStyleSetBorrowed set);

// Reparents style to the new parents.
ComputedStyleStrong Servo_ReparentStyle(
    ComputedStyleBorrowed style_to_reparent, ComputedStyleBorrowed parent_style,
    ComputedStyleBorrowed parent_style_ignoring_first_line,
    ComputedStyleBorrowed layout_parent_style,
    // element is null if there is no content node involved, or if it's not an
    // element.
    RawGeckoElementBorrowedOrNull element, RawServoStyleSetBorrowed set);

// Use ServoStyleSet::PrepareAndTraverseSubtree instead of calling this
// directly
bool Servo_TraverseSubtree(RawGeckoElementBorrowed root,
                           RawServoStyleSetBorrowed set,
                           const mozilla::ServoElementSnapshotTable* snapshots,
                           mozilla::ServoTraversalFlags flags);

// Assert that the tree has no pending or unconsumed restyles.
void Servo_AssertTreeIsClean(RawGeckoElementBorrowed root);

// Returns true if the current thread is a Servo parallel worker thread.
bool Servo_IsWorkerThread();

// Checks whether the rule tree has crossed its threshold for unused rule nodes,
// and if so, frees them.
void Servo_MaybeGCRuleTree(RawServoStyleSetBorrowed set);

// Returns computed values for the given element without any animations rules.
ComputedStyleStrong Servo_StyleSet_GetBaseComputedValuesForElement(
    RawServoStyleSetBorrowed set, RawGeckoElementBorrowed element,
    ComputedStyleBorrowed existing_style,
    const mozilla::ServoElementSnapshotTable* snapshots);

// Returns computed values for the given element by adding an animation value.
ComputedStyleStrong Servo_StyleSet_GetComputedValuesByAddingAnimation(
    RawServoStyleSetBorrowed set, RawGeckoElementBorrowed element,
    ComputedStyleBorrowed existing_style,
    const mozilla::ServoElementSnapshotTable* snapshots,
    RawServoAnimationValueBorrowed animation);

// For canvas font.
void Servo_SerializeFontValueForCanvas(
    RawServoDeclarationBlockBorrowed declarations, nsAString* buffer);

// GetComputedStyle APIs.
bool Servo_GetCustomPropertyValue(ComputedStyleBorrowed computed_values,
                                  const nsAString* name, nsAString* value);

uint32_t Servo_GetCustomPropertiesCount(ComputedStyleBorrowed computed_values);

bool Servo_GetCustomPropertyNameAt(ComputedStyleBorrowed, uint32_t index,
                                   nsAString* name);

void Servo_GetPropertyValue(ComputedStyleBorrowed computed_values,
                            nsCSSPropertyID property, nsAString* value);

void Servo_ProcessInvalidations(
    RawServoStyleSetBorrowed set, RawGeckoElementBorrowed element,
    const mozilla::ServoElementSnapshotTable* snapshots);

bool Servo_HasPendingRestyleAncestor(RawGeckoElementBorrowed element);

void Servo_CssUrlData_GetSerialization(RawServoCssUrlDataBorrowed url,
                                       uint8_t const** chars, uint32_t* len);

RawGeckoURLExtraDataBorrowedMut Servo_CssUrlData_GetExtraData(
    RawServoCssUrlDataBorrowed url);

bool Servo_CssUrlData_IsLocalRef(RawServoCssUrlDataBorrowed url);

// CSS parsing utility functions.

bool Servo_IsValidCSSColor(const nsAString* value);

bool Servo_ComputeColor(RawServoStyleSetBorrowedOrNull set,
                        nscolor current_color, const nsAString* value,
                        nscolor* result_color, bool* was_current_color,
                        mozilla::css::Loader* loader);

bool Servo_IntersectionObserverRootMargin_Parse(
    const nsAString* value,
    mozilla::StyleIntersectionObserverRootMargin* result);

void Servo_IntersectionObserverRootMargin_ToString(
    const mozilla::StyleIntersectionObserverRootMargin*, nsAString* result);

// Returning false means the parsed transform contains relative lengths or
// percentage value, so we cannot compute the matrix. In this case, we keep
// |result| and |contains_3d_transform| as-is.
bool Servo_ParseTransformIntoMatrix(const nsAString* value,
                                    bool* contains_3d_transform,
                                    RawGeckoGfxMatrix4x4* result);

bool Servo_ParseFontShorthandForMatching(
    const nsAString* value, RawGeckoURLExtraData* data,
    RefPtr<mozilla::SharedFontList>* family,
    // We use ComputedFontStyleDescriptor just for convenience,
    // but the two values of Oblique are the same.
    mozilla::StyleComputedFontStyleDescriptor* style, float* stretch,
    float* weight);

nsCSSPropertyID Servo_ResolveLogicalProperty(nsCSSPropertyID,
                                             ComputedStyleBorrowed);

nsCSSPropertyID Servo_Property_LookupEnabledForAllContent(
    const nsACString* name);

const uint8_t* Servo_Property_GetName(nsCSSPropertyID, uint32_t* out_length);
bool Servo_Property_IsShorthand(const nsACString* name, bool* found);
bool Servo_Property_IsInherited(const nsACString* name);

bool Servo_Property_SupportsType(const nsACString* name, uint32_t ty,
                                 bool* found);

void Servo_Property_GetCSSValuesForProperty(const nsACString* name, bool* found,
                                            nsTArray<nsString>* result);

uint64_t Servo_PseudoClass_GetStates(const nsACString* name);

StyleUseCounters* Servo_UseCounters_Create();
void Servo_UseCounters_Drop(StyleUseCountersOwned);

void Servo_UseCounters_Merge(StyleUseCountersBorrowed doc_counters,
                             StyleUseCountersBorrowed sheet_counters);

bool Servo_IsCssPropertyRecordedInUseCounter(StyleUseCountersBorrowed,
                                             const nsACString* property,
                                             bool* out_known_prop);

RawServoQuotesStrong Servo_Quotes_GetInitialValue();
bool Servo_Quotes_Equal(RawServoQuotesBorrowed a, RawServoQuotesBorrowed b);

void Servo_Quotes_GetQuote(RawServoQuotesBorrowed quotes, int32_t depth,
                           mozilla::StyleContentType quote_type,
                           nsAString* result);

// AddRef / Release functions
#define SERVO_ARC_TYPE(name_, type_)            \
  void Servo_##name_##_AddRef(type_##Borrowed); \
  void Servo_##name_##_Release(type_##Borrowed);
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

}  // extern "C"

#endif  // mozilla_ServoBindings_h
