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
void Servo_Element_ClearData(const mozilla::dom::Element* node);

size_t Servo_Element_SizeOfExcludingThisAndCVs(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    mozilla::SeenPtrs* seen_ptrs, const mozilla::dom::Element* node);

bool Servo_Element_HasPrimaryComputedValues(const mozilla::dom::Element* node);

ComputedStyleStrong Servo_Element_GetPrimaryComputedValues(
    const mozilla::dom::Element* node);

bool Servo_Element_HasPseudoComputedValues(const mozilla::dom::Element* node,
                                           size_t index);

ComputedStyleStrong Servo_Element_GetPseudoComputedValues(
    const mozilla::dom::Element* node, size_t index);

bool Servo_Element_IsDisplayNone(const mozilla::dom::Element* element);
bool Servo_Element_IsDisplayContents(const mozilla::dom::Element* element);

bool Servo_Element_IsPrimaryStyleReusedViaRuleNode(
    const mozilla::dom::Element* element);

void Servo_InvalidateStyleForDocStateChanges(
    const mozilla::dom::Element* root, const RawServoStyleSet* doc_styles,
    const nsTArray<const RawServoAuthorStyles*>* non_document_styles,
    uint64_t aStatesChanged);

// Styleset and Stylesheet management

RawServoStyleSheetContentsStrong Servo_StyleSheet_FromUTF8Bytes(
    mozilla::css::Loader* loader, mozilla::StyleSheet* gecko_stylesheet,
    mozilla::css::SheetLoadData* load_data, const nsACString* bytes,
    mozilla::css::SheetParsingMode parsing_mode,
    mozilla::URLExtraData* extra_data, uint32_t line_number_offset,
    nsCompatibility quirks_mode,
    mozilla::css::LoaderReusableStyleSheets* reusable_sheets,
    const StyleUseCounters* use_counters);

void Servo_StyleSheet_FromUTF8BytesAsync(
    mozilla::css::SheetLoadDataHolder* load_data,
    mozilla::URLExtraData* extra_data, const nsACString* bytes,
    mozilla::css::SheetParsingMode parsing_mode, uint32_t line_number_offset,
    nsCompatibility quirks_mode, bool should_record_use_counters);

RawServoStyleSheetContentsStrong Servo_StyleSheet_Empty(
    mozilla::css::SheetParsingMode parsing_mode);

bool Servo_StyleSheet_HasRules(const RawServoStyleSheetContents* sheet);

ServoCssRulesStrong Servo_StyleSheet_GetRules(
    const RawServoStyleSheetContents* sheet);

RawServoStyleSheetContentsStrong Servo_StyleSheet_Clone(
    const RawServoStyleSheetContents* sheet,
    const mozilla::StyleSheet* reference_sheet);

size_t Servo_StyleSheet_SizeOfIncludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    const RawServoStyleSheetContents* sheet);

void Servo_StyleSheet_GetSourceMapURL(const RawServoStyleSheetContents* sheet,
                                      nsAString* result);

void Servo_StyleSheet_GetSourceURL(const RawServoStyleSheetContents* sheet,
                                   nsAString* result);

// We'd like to return `OriginFlags` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
uint8_t Servo_StyleSheet_GetOrigin(const RawServoStyleSheetContents* sheet);

RawServoStyleSet* Servo_StyleSet_Init(const nsPresContext* pres_context);
void Servo_StyleSet_RebuildCachedData(const RawServoStyleSet* set);

// We'd like to return `OriginFlags` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
mozilla::MediumFeaturesChangedResult Servo_StyleSet_MediumFeaturesChanged(
    const RawServoStyleSet* document_set,
    nsTArray<RawServoAuthorStyles*>* non_document_sets,
    bool may_affect_default_style);

void Servo_StyleSet_Drop(RawServoStyleSetOwned set);
void Servo_StyleSet_CompatModeChanged(const RawServoStyleSet* raw_data);

void Servo_StyleSet_AppendStyleSheet(const RawServoStyleSet* set,
                                     const mozilla::StyleSheet* gecko_sheet);

void Servo_StyleSet_RemoveStyleSheet(const RawServoStyleSet* set,
                                     const mozilla::StyleSheet* gecko_sheet);

void Servo_StyleSet_InsertStyleSheetBefore(
    const RawServoStyleSet* set, const mozilla::StyleSheet* gecko_sheet,
    const mozilla::StyleSheet* before);

void Servo_StyleSet_FlushStyleSheets(
    const RawServoStyleSet* set, const mozilla::dom::Element* doc_elem,
    const mozilla::ServoElementSnapshotTable* snapshots);

void Servo_StyleSet_SetAuthorStyleDisabled(const RawServoStyleSet* set,
                                           bool author_style_disabled);

void Servo_StyleSet_NoteStyleSheetsChanged(
    const RawServoStyleSet* set, mozilla::OriginFlags changed_origins);

bool Servo_StyleSet_GetKeyframesForName(
    const RawServoStyleSet* set, const mozilla::dom::Element* element,
    const mozilla::ComputedStyle* style, nsAtom* name,
    const nsTimingFunction* timing_function,
    nsTArray<mozilla::Keyframe>* keyframe_list);

void Servo_StyleSet_GetFontFaceRules(const RawServoStyleSet* set,
                                     nsTArray<nsFontFaceRuleContainer>*);

const RawServoCounterStyleRule* Servo_StyleSet_GetCounterStyleRule(
    const RawServoStyleSet* set, nsAtom* name);

// This function may return nullptr or gfxFontFeatureValueSet with zero
// references.
gfxFontFeatureValueSet* Servo_StyleSet_BuildFontFeatureValueSet(
    const RawServoStyleSet* set);

ComputedStyleStrong Servo_StyleSet_ResolveForDeclarations(
    const RawServoStyleSet* set, const mozilla::ComputedStyle* parent_style,
    const RawServoDeclarationBlock* declarations);

void Servo_SelectorList_Drop(RawServoSelectorList*);
RawServoSelectorList* Servo_SelectorList_Parse(const nsACString* selector_list);
RawServoSourceSizeList* Servo_SourceSizeList_Parse(const nsACString* value);

int32_t Servo_SourceSizeList_Evaluate(const RawServoStyleSet* set,
                                      const RawServoSourceSizeList*);

void Servo_SourceSizeList_Drop(RawServoSourceSizeList*);

bool Servo_SelectorList_Matches(const mozilla::dom::Element*,
                                const RawServoSelectorList*);

const mozilla::dom::Element* Servo_SelectorList_Closest(
    const mozilla::dom::Element*, const RawServoSelectorList*);

const mozilla::dom::Element* Servo_SelectorList_QueryFirst(
    const nsINode*, const RawServoSelectorList*, bool may_use_invalidation);

void Servo_SelectorList_QueryAll(const nsINode*, const RawServoSelectorList*,
                                 nsSimpleContentList* content_list,
                                 bool may_use_invalidation);

void Servo_StyleSet_AddSizeOfExcludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    mozilla::ServoStyleSetSizes* sizes, const RawServoStyleSet* set);

void Servo_UACache_AddSizeOf(mozilla::MallocSizeOf malloc_size_of,
                             mozilla::MallocSizeOf malloc_enclosing_size_of,
                             mozilla::ServoStyleSetSizes* sizes);

// AuthorStyles

RawServoAuthorStyles* Servo_AuthorStyles_Create();
void Servo_AuthorStyles_Drop(RawServoAuthorStyles*);

void Servo_AuthorStyles_AppendStyleSheet(RawServoAuthorStyles*,
                                         const mozilla::StyleSheet*);

void Servo_AuthorStyles_RemoveStyleSheet(RawServoAuthorStyles*,
                                         const mozilla::StyleSheet*);

void Servo_AuthorStyles_InsertStyleSheetBefore(
    RawServoAuthorStyles*, const mozilla::StyleSheet* sheet,
    const mozilla::StyleSheet* before);

void Servo_AuthorStyles_ForceDirty(RawServoAuthorStyles*);

// TODO(emilio): This will need to take an element and a master style set to
// implement invalidation for Shadow DOM.
void Servo_AuthorStyles_Flush(RawServoAuthorStyles*,
                              const RawServoStyleSet* document_styles);

size_t Servo_AuthorStyles_SizeOfIncludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    const RawServoAuthorStyles* self);

void Servo_ComputedStyle_AddRef(const mozilla::ComputedStyle* ctx);

void Servo_ComputedStyle_Release(const mozilla::ComputedStyle* ctx);

bool Servo_StyleSet_MightHaveAttributeDependency(
    const RawServoStyleSet* set, const mozilla::dom::Element* element,
    nsAtom* local_name);

bool Servo_StyleSet_HasStateDependency(const RawServoStyleSet* set,
                                       const mozilla::dom::Element* element,
                                       uint64_t state);

bool Servo_StyleSet_HasDocumentStateDependency(const RawServoStyleSet* set,
                                               uint64_t state);

// CSSRuleList

void Servo_CssRules_ListTypes(const ServoCssRules*,
                              const nsTArray<uintptr_t>* result);

nsresult Servo_CssRules_InsertRule(const ServoCssRules*,
                                   const RawServoStyleSheetContents* sheet,
                                   const nsACString* rule, uint32_t index,
                                   bool nested, mozilla::css::Loader* loader,
                                   mozilla::StyleSheet* gecko_stylesheet,
                                   uint16_t* rule_type);

nsresult Servo_CssRules_DeleteRule(const ServoCssRules* rules, uint32_t index);

// CSS Rules

#define BASIC_RULE_FUNCS_WITHOUT_GETTER(type_)                            \
  void Servo_##type_##_Debug(const RawServo##type_*, nsACString* result); \
  void Servo_##type_##_GetCssText(const RawServo##type_*, nsAString* result);

#define BASIC_RULE_FUNCS(type_)                                   \
  RawServo##type_##RuleStrong Servo_CssRules_Get##type_##RuleAt(  \
      const ServoCssRules* rules, uint32_t index, uint32_t* line, \
      uint32_t* column);                                          \
  BASIC_RULE_FUNCS_WITHOUT_GETTER(type_##Rule)

#define GROUP_RULE_FUNCS(type_)                     \
  BASIC_RULE_FUNCS(type_)                           \
  ServoCssRulesStrong Servo_##type_##Rule_GetRules( \
      const RawServo##type_##Rule* rule);

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

using Matrix4x4Components = float[16];

RawServoDeclarationBlockStrong Servo_StyleRule_GetStyle(
    const RawServoStyleRule*);

void Servo_StyleRule_SetStyle(const RawServoStyleRule* rule,
                              const RawServoDeclarationBlock* declarations);

void Servo_StyleRule_GetSelectorText(const RawServoStyleRule* rule,
                                     nsAString* result);

void Servo_StyleRule_GetSelectorTextAtIndex(const RawServoStyleRule* rule,
                                            uint32_t index, nsAString* result);

void Servo_StyleRule_GetSpecificityAtIndex(const RawServoStyleRule* rule,
                                           uint32_t index,
                                           uint64_t* specificity);

void Servo_StyleRule_GetSelectorCount(const RawServoStyleRule* rule,
                                      uint32_t* count);

bool Servo_StyleRule_SelectorMatchesElement(
    const RawServoStyleRule*, const mozilla::dom::Element*, uint32_t index,
    mozilla::PseudoStyleType pseudo_type);

bool Servo_StyleRule_SetSelectorText(const RawServoStyleSheetContents* sheet,
                                     const RawServoStyleRule* rule,
                                     const nsAString* text);

void Servo_ImportRule_GetHref(const RawServoImportRule* rule,
                              nsAString* result);

const mozilla::StyleSheet* Servo_ImportRule_GetSheet(
    const RawServoImportRule* rule);

void Servo_ImportRule_SetSheet(const RawServoImportRule* rule,
                               mozilla::StyleSheet* sheet);

void Servo_Keyframe_GetKeyText(const RawServoKeyframe* keyframe,
                               nsAString* result);

// Returns whether it successfully changes the key text.
bool Servo_Keyframe_SetKeyText(const RawServoKeyframe* keyframe,
                               const nsACString* text);

RawServoDeclarationBlockStrong Servo_Keyframe_GetStyle(
    const RawServoKeyframe* keyframe);

void Servo_Keyframe_SetStyle(const RawServoKeyframe* keyframe,
                             const RawServoDeclarationBlock* declarations);

nsAtom* Servo_KeyframesRule_GetName(const RawServoKeyframesRule* rule);

// This method takes an addrefed nsAtom.
void Servo_KeyframesRule_SetName(const RawServoKeyframesRule* rule,
                                 nsAtom* name);

uint32_t Servo_KeyframesRule_GetCount(const RawServoKeyframesRule* rule);

RawServoKeyframeStrong Servo_KeyframesRule_GetKeyframeAt(
    const RawServoKeyframesRule* rule, uint32_t index, uint32_t* line,
    uint32_t* column);

// Returns the index of the rule, max value of uint32_t if nothing found.
uint32_t Servo_KeyframesRule_FindRule(const RawServoKeyframesRule* rule,
                                      const nsACString* key);

// Returns whether it successfully appends the rule.
bool Servo_KeyframesRule_AppendRule(const RawServoKeyframesRule* rule,
                                    const RawServoStyleSheetContents* sheet,
                                    const nsACString* css);

void Servo_KeyframesRule_DeleteRule(const RawServoKeyframesRule* rule,
                                    uint32_t index);

RawServoMediaListStrong Servo_MediaRule_GetMedia(const RawServoMediaRule* rule);

nsAtom* Servo_NamespaceRule_GetPrefix(const RawServoNamespaceRule* rule);
nsAtom* Servo_NamespaceRule_GetURI(const RawServoNamespaceRule* rule);

RawServoDeclarationBlockStrong Servo_PageRule_GetStyle(
    const RawServoPageRule* rule);

void Servo_PageRule_SetStyle(const RawServoPageRule* rule,
                             const RawServoDeclarationBlock* declarations);

void Servo_SupportsRule_GetConditionText(const RawServoSupportsRule* rule,
                                         nsAString* result);

void Servo_MozDocumentRule_GetConditionText(const RawServoMozDocumentRule* rule,
                                            nsAString* result);

void Servo_FontFeatureValuesRule_GetFontFamily(
    const RawServoFontFeatureValuesRule* rule, nsAString* result);

void Servo_FontFeatureValuesRule_GetValueText(
    const RawServoFontFeatureValuesRule* rule, nsAString* result);

RawServoFontFaceRuleStrong Servo_FontFaceRule_CreateEmpty();

RawServoFontFaceRuleStrong Servo_FontFaceRule_Clone(
    const RawServoFontFaceRule* rule);

void Servo_FontFaceRule_GetSourceLocation(const RawServoFontFaceRule* rule,
                                          uint32_t* line, uint32_t* column);

uint32_t Servo_FontFaceRule_Length(const RawServoFontFaceRule* rule);

nsCSSFontDesc Servo_FontFaceRule_IndexGetter(const RawServoFontFaceRule* rule,
                                             uint32_t index);

void Servo_FontFaceRule_GetDeclCssText(const RawServoFontFaceRule* rule,
                                       nsAString* result);

bool Servo_FontFaceRule_GetFontWeight(
    const RawServoFontFaceRule* rule,
    mozilla::StyleComputedFontWeightRange* out);

bool Servo_FontFaceRule_GetFontDisplay(const RawServoFontFaceRule* rule,
                                       mozilla::StyleFontDisplay* out);

bool Servo_FontFaceRule_GetFontStyle(
    const RawServoFontFaceRule* rule,
    mozilla::StyleComputedFontStyleDescriptor* out);

bool Servo_FontFaceRule_GetFontStretch(
    const RawServoFontFaceRule* rule,
    mozilla::StyleComputedFontStretchRange* out);

bool Servo_FontFaceRule_GetFontLanguageOverride(
    const RawServoFontFaceRule* rule, mozilla::StyleFontLanguageOverride* out);

nsAtom* Servo_FontFaceRule_GetFamilyName(const RawServoFontFaceRule* rule);

const mozilla::StyleUnicodeRange* Servo_FontFaceRule_GetUnicodeRanges(
    const RawServoFontFaceRule* rule, size_t* out_len);

void Servo_FontFaceRule_GetSources(
    const RawServoFontFaceRule* rule,
    nsTArray<mozilla::StyleFontFaceSourceListComponent>* components);

void Servo_FontFaceRule_GetVariationSettings(
    const RawServoFontFaceRule* rule,
    nsTArray<mozilla::gfx::FontVariation>* out);

void Servo_FontFaceRule_GetFeatureSettings(const RawServoFontFaceRule* rule,
                                           nsTArray<gfxFontFeature>* out);

void Servo_FontFaceRule_GetDescriptorCssText(const RawServoFontFaceRule* rule,
                                             nsCSSFontDesc desc,
                                             nsAString* result);

bool Servo_FontFaceRule_SetDescriptor(const RawServoFontFaceRule* rule,
                                      nsCSSFontDesc desc,
                                      const nsACString* value,
                                      mozilla::URLExtraData* data);

void Servo_FontFaceRule_ResetDescriptor(const RawServoFontFaceRule* rule,
                                        nsCSSFontDesc desc);

nsAtom* Servo_CounterStyleRule_GetName(const RawServoCounterStyleRule* rule);

bool Servo_CounterStyleRule_SetName(const RawServoCounterStyleRule* rule,
                                    const nsACString* name);

uint32_t Servo_CounterStyleRule_GetGeneration(
    const RawServoCounterStyleRule* rule);

uint8_t Servo_CounterStyleRule_GetSystem(const RawServoCounterStyleRule* rule);

nsAtom* Servo_CounterStyleRule_GetExtended(
    const RawServoCounterStyleRule* rule);

int32_t Servo_CounterStyleRule_GetFixedFirstValue(
    const RawServoCounterStyleRule* rule);

nsAtom* Servo_CounterStyleRule_GetFallback(
    const RawServoCounterStyleRule* rule);

void Servo_CounterStyleRule_GetDescriptor(const RawServoCounterStyleRule* rule,
                                          nsCSSCounterDesc desc,
                                          nsCSSValue* result);

void Servo_CounterStyleRule_GetDescriptorCssText(
    const RawServoCounterStyleRule* rule, nsCSSCounterDesc desc,
    nsAString* result);

bool Servo_CounterStyleRule_SetDescriptor(const RawServoCounterStyleRule* rule,
                                          nsCSSCounterDesc desc,
                                          const nsACString* value);

// Animations API

RawServoDeclarationBlockStrong Servo_ParseProperty(
    nsCSSPropertyID property, const nsACString* value,
    mozilla::URLExtraData* data, mozilla::ParsingMode parsing_mode,
    nsCompatibility quirks_mode, mozilla::css::Loader* loader);

bool Servo_ParseEasing(const nsAString* easing, mozilla::URLExtraData* data,
                       nsTimingFunction* output);

void Servo_SerializeEasing(const nsTimingFunction* easing, nsAString* output);

void Servo_SerializeBorderRadius(const mozilla::StyleBorderRadius*, nsAString*);

void Servo_GetComputedKeyframeValues(
    const nsTArray<mozilla::Keyframe>* keyframes,
    const mozilla::dom::Element* element, const mozilla::ComputedStyle* style,
    const RawServoStyleSet* set,
    nsTArray<mozilla::ComputedKeyframeValues>* result);

RawServoAnimationValueStrong Servo_ComputedValues_ExtractAnimationValue(
    const mozilla::ComputedStyle* computed_values, nsCSSPropertyID property);

bool Servo_ComputedValues_SpecifiesAnimationsOrTransitions(
    const mozilla::ComputedStyle* computed_values);

bool Servo_Property_IsAnimatable(nsCSSPropertyID property);
bool Servo_Property_IsTransitionable(nsCSSPropertyID property);
bool Servo_Property_IsDiscreteAnimatable(nsCSSPropertyID property);

void Servo_GetProperties_Overriding_Animation(const mozilla::dom::Element*,
                                              const nsTArray<nsCSSPropertyID>*,
                                              nsCSSPropertyIDSet*);

void Servo_MatrixTransform_Operate(
    nsStyleTransformMatrix::MatrixTransformOperator matrix_operator,
    const Matrix4x4Components* from, const Matrix4x4Components* to,
    double progress, Matrix4x4Components* result);

void Servo_GetAnimationValues(
    const RawServoDeclarationBlock* declarations,
    const mozilla::dom::Element* element, const mozilla::ComputedStyle* style,
    const RawServoStyleSet* style_set,
    nsTArray<RefPtr<RawServoAnimationValue>>* animation_values);

// AnimationValues handling

RawServoAnimationValueStrong Servo_AnimationValues_Interpolate(
    const RawServoAnimationValue* from, const RawServoAnimationValue* to,
    double progress);

bool Servo_AnimationValues_IsInterpolable(const RawServoAnimationValue* from,
                                          const RawServoAnimationValue* to);

RawServoAnimationValueStrong Servo_AnimationValues_Add(
    const RawServoAnimationValue* a, const RawServoAnimationValue* b);

RawServoAnimationValueStrong Servo_AnimationValues_Accumulate(
    const RawServoAnimationValue* a, const RawServoAnimationValue* b,
    uint64_t count);

RawServoAnimationValueStrong Servo_AnimationValues_GetZeroValue(
    const RawServoAnimationValue* value_to_match);

double Servo_AnimationValues_ComputeDistance(const RawServoAnimationValue* from,
                                             const RawServoAnimationValue* to);

void Servo_AnimationValue_Serialize(const RawServoAnimationValue* value,
                                    nsCSSPropertyID property,
                                    nsAString* buffer);

nscolor Servo_AnimationValue_GetColor(const RawServoAnimationValue* value,
                                      nscolor foregroundColor);
RawServoAnimationValueStrong Servo_AnimationValue_Color(nsCSSPropertyID,
                                                        nscolor);

float Servo_AnimationValue_GetOpacity(const RawServoAnimationValue* value);
RawServoAnimationValueStrong Servo_AnimationValue_Opacity(float);

nsCSSPropertyID Servo_AnimationValue_GetTransform(
    const RawServoAnimationValue* value, RefPtr<nsCSSValueSharedList>* list);

RawServoAnimationValueStrong Servo_AnimationValue_Transform(
    nsCSSPropertyID property, const nsCSSValueSharedList& list);

bool Servo_AnimationValue_DeepEqual(const RawServoAnimationValue*,
                                    const RawServoAnimationValue*);

RawServoDeclarationBlockStrong Servo_AnimationValue_Uncompute(
    const RawServoAnimationValue* value);

RawServoAnimationValueStrong Servo_AnimationValue_Compute(
    const mozilla::dom::Element* element,
    const RawServoDeclarationBlock* declarations,
    const mozilla::ComputedStyle* style, const RawServoStyleSet* raw_data);

// Style attribute

RawServoDeclarationBlockStrong Servo_ParseStyleAttribute(
    const nsACString* data, mozilla::URLExtraData* extra_data,
    nsCompatibility quirks_mode, mozilla::css::Loader* loader);

RawServoDeclarationBlockStrong Servo_DeclarationBlock_CreateEmpty();

RawServoDeclarationBlockStrong Servo_DeclarationBlock_Clone(
    const RawServoDeclarationBlock* declarations);

bool Servo_DeclarationBlock_Equals(const RawServoDeclarationBlock* a,
                                   const RawServoDeclarationBlock* b);

void Servo_DeclarationBlock_GetCssText(
    const RawServoDeclarationBlock* declarations, nsAString* result);

void Servo_DeclarationBlock_SerializeOneValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    nsAString* buffer, const mozilla::ComputedStyle* computed_values,
    const RawServoDeclarationBlock* custom_properties);

uint32_t Servo_DeclarationBlock_Count(
    const RawServoDeclarationBlock* declarations);

bool Servo_DeclarationBlock_GetNthProperty(
    const RawServoDeclarationBlock* declarations, uint32_t index,
    nsAString* result);

void Servo_DeclarationBlock_GetPropertyValue(
    const RawServoDeclarationBlock* declarations, const nsACString* property,
    nsAString* value);

void Servo_DeclarationBlock_GetPropertyValueById(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    nsAString* value);

bool Servo_DeclarationBlock_GetPropertyIsImportant(
    const RawServoDeclarationBlock* declarations, const nsACString* property);

bool Servo_DeclarationBlock_SetProperty(
    const RawServoDeclarationBlock* declarations, const nsACString* property,
    const nsACString* value, bool is_important, mozilla::URLExtraData* data,
    mozilla::ParsingMode parsing_mode, nsCompatibility quirks_mode,
    mozilla::css::Loader* loader, mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_SetPropertyToAnimationValue(
    const RawServoDeclarationBlock* declarations,
    const RawServoAnimationValue* animation_value);

bool Servo_DeclarationBlock_SetPropertyById(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    const nsACString* value, bool is_important, mozilla::URLExtraData* data,
    mozilla::ParsingMode parsing_mode, nsCompatibility quirks_mode,
    mozilla::css::Loader* loader, mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_RemoveProperty(
    const RawServoDeclarationBlock* declarations, const nsACString* property,
    mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_RemovePropertyById(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    mozilla::DeclarationBlockMutationClosure);

bool Servo_DeclarationBlock_HasCSSWideKeyword(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property);

// Compose animation value for a given property.
// |base_values| is nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>.
// We use const RawServoAnimationValueTable* to avoid exposing
// nsRefPtrHashtable in FFI.
void Servo_AnimationCompose(
    RawServoAnimationValueMap* animation_values,
    const RawServoAnimationValueTable* base_values, nsCSSPropertyID property,
    const mozilla::AnimationPropertySegment* animation_segment,
    const mozilla::AnimationPropertySegment* last_segment,
    const mozilla::ComputedTiming* computed_timing,
    mozilla::dom::IterationCompositeOperation iter_composite);

// Calculate the result of interpolating given animation segment at the given
// progress and current iteration.
// This includes combining the segment endpoints with the underlying value
// and/or last value depending the composite modes specified on the
// segment endpoints and the supplied iteration composite mode.
// The caller is responsible for providing an underlying value and
// last value in all situations where there are needed.
RawServoAnimationValueStrong Servo_ComposeAnimationSegment(
    const mozilla::AnimationPropertySegment* animation_segment,
    const RawServoAnimationValue* underlying_value,
    const RawServoAnimationValue* last_value,
    mozilla::dom::IterationCompositeOperation iter_composite, double progress,
    uint64_t current_iteration);

// presentation attributes

bool Servo_DeclarationBlock_PropertyIsSet(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property);

void Servo_DeclarationBlock_SetIdentStringValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    nsAtom* value);

void Servo_DeclarationBlock_SetKeywordValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    int32_t value);

void Servo_DeclarationBlock_SetIntValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    int32_t value);

void Servo_DeclarationBlock_SetPixelValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    float value);

void Servo_DeclarationBlock_SetLengthValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    float value, nsCSSUnit unit);

void Servo_DeclarationBlock_SetNumberValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    float value);

void Servo_DeclarationBlock_SetPercentValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    float value);

void Servo_DeclarationBlock_SetAutoValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property);

void Servo_DeclarationBlock_SetCurrentColor(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property);

void Servo_DeclarationBlock_SetColorValue(
    const RawServoDeclarationBlock* declarations, nsCSSPropertyID property,
    nscolor value);

void Servo_DeclarationBlock_SetFontFamily(
    const RawServoDeclarationBlock* declarations, const nsAString& value);

void Servo_DeclarationBlock_SetTextDecorationColorOverride(
    const RawServoDeclarationBlock* declarations);

void Servo_DeclarationBlock_SetBackgroundImage(
    const RawServoDeclarationBlock* declarations, const nsAString& value,
    mozilla::URLExtraData* extra_data);

// MediaList

RawServoMediaListStrong Servo_MediaList_Create();

RawServoMediaListStrong Servo_MediaList_DeepClone(
    const RawServoMediaList* list);

bool Servo_MediaList_Matches(const RawServoMediaList* list,
                             const RawServoStyleSet* set);

void Servo_MediaList_GetText(const RawServoMediaList* list, nsAString* result);

void Servo_MediaList_SetText(const RawServoMediaList* list,
                             const nsACString* text,
                             mozilla::dom::CallerType aCallerType);

uint32_t Servo_MediaList_GetLength(const RawServoMediaList* list);

bool Servo_MediaList_GetMediumAt(const RawServoMediaList* list, uint32_t index,
                                 nsAString* result);

void Servo_MediaList_AppendMedium(const RawServoMediaList* list,
                                  const nsACString* new_medium);

bool Servo_MediaList_DeleteMedium(const RawServoMediaList* list,
                                  const nsACString* old_medium);

size_t Servo_MediaList_SizeOfIncludingThis(
    mozilla::MallocSizeOf malloc_size_of,
    mozilla::MallocSizeOf malloc_enclosing_size_of,
    const RawServoMediaList* list);

// CSS supports();

bool Servo_CSSSupports2(const nsACString* name, const nsACString* value);
bool Servo_CSSSupports(const nsACString* cond);

// Computed style data

ComputedStyleStrong Servo_ComputedValues_GetForAnonymousBox(
    const mozilla::ComputedStyle* parent_style_or_null,
    mozilla::PseudoStyleType, const RawServoStyleSet* set);

ComputedStyleStrong Servo_ComputedValues_Inherit(
    const RawServoStyleSet*, mozilla::PseudoStyleType,
    const mozilla::ComputedStyle* parent_style, mozilla::InheritTarget);

// Gets the source style rules for the computed values. This returns
// the result via rules, which would include a list of unowned pointers
// to RawServoStyleRule.
void Servo_ComputedValues_GetStyleRuleList(
    const mozilla::ComputedStyle* values,
    nsTArray<const RawServoStyleRule*>* rules);

// Initialize Servo components. Should be called exactly once at startup.
void Servo_Initialize(mozilla::URLExtraData* dummy_url_data);

// Initialize Servo on a cooperative Quantum DOM thread.
void Servo_InitializeCooperativeThread();

// Shut down Servo components. Should be called exactly once at shutdown.
void Servo_Shutdown();

// Restyle and change hints.
void Servo_NoteExplicitHints(const mozilla::dom::Element*, mozilla::RestyleHint,
                             nsChangeHint);

// We'd like to return `nsChangeHint` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
uint32_t Servo_TakeChangeHint(const mozilla::dom::Element* element,
                              bool* was_restyled);

ComputedStyleStrong Servo_ResolveStyle(const mozilla::dom::Element* element,
                                       const RawServoStyleSet* set);

ComputedStyleStrong Servo_ResolvePseudoStyle(
    const mozilla::dom::Element* element, mozilla::PseudoStyleType pseudo_type,
    bool is_probe, const mozilla::ComputedStyle* inherited_style,
    const RawServoStyleSet* set);

ComputedStyleStrong Servo_ComputedValues_ResolveXULTreePseudoStyle(
    const mozilla::dom::Element* element, nsAtom* pseudo_tag,
    const mozilla::ComputedStyle* inherited_style,
    const mozilla::AtomArray* input_word, const RawServoStyleSet* set);

void Servo_SetExplicitStyle(const mozilla::dom::Element* element,
                            const mozilla::ComputedStyle* primary_style);

bool Servo_HasAuthorSpecifiedRules(const RawServoStyleSet* set,
                                   const mozilla::ComputedStyle* style,
                                   const mozilla::dom::Element* element,
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
    const mozilla::dom::Element* element, mozilla::PseudoStyleType pseudo_type,
    mozilla::StyleRuleInclusion rule_inclusion,
    const mozilla::ServoElementSnapshotTable* snapshots,
    const RawServoStyleSet* set);

// Reparents style to the new parents.
ComputedStyleStrong Servo_ReparentStyle(
    const mozilla::ComputedStyle* style_to_reparent,
    const mozilla::ComputedStyle* parent_style,
    const mozilla::ComputedStyle* parent_style_ignoring_first_line,
    const mozilla::ComputedStyle* layout_parent_style,
    // element is null if there is no content node involved, or if it's not an
    // element.
    const mozilla::dom::Element* element, const RawServoStyleSet* set);

// Use ServoStyleSet::PrepareAndTraverseSubtree instead of calling this
// directly
bool Servo_TraverseSubtree(const mozilla::dom::Element* root,
                           const RawServoStyleSet* set,
                           const mozilla::ServoElementSnapshotTable* snapshots,
                           mozilla::ServoTraversalFlags flags);

// Assert that the tree has no pending or unconsumed restyles.
void Servo_AssertTreeIsClean(const mozilla::dom::Element* root);

// Returns true if the current thread is a Servo parallel worker thread.
bool Servo_IsWorkerThread();

// Checks whether the rule tree has crossed its threshold for unused rule nodes,
// and if so, frees them.
void Servo_MaybeGCRuleTree(const RawServoStyleSet* set);

// Returns computed values for the given element without any animations rules.
ComputedStyleStrong Servo_StyleSet_GetBaseComputedValuesForElement(
    const RawServoStyleSet* set, const mozilla::dom::Element* element,
    const mozilla::ComputedStyle* existing_style,
    const mozilla::ServoElementSnapshotTable* snapshots);

// Returns computed values for the given element by adding an animation value.
ComputedStyleStrong Servo_StyleSet_GetComputedValuesByAddingAnimation(
    const RawServoStyleSet* set, const mozilla::dom::Element* element,
    const mozilla::ComputedStyle* existing_style,
    const mozilla::ServoElementSnapshotTable* snapshots,
    const RawServoAnimationValue* animation);

// For canvas font.
void Servo_SerializeFontValueForCanvas(
    const RawServoDeclarationBlock* declarations, nsAString* buffer);

// GetComputedStyle APIs.
bool Servo_GetCustomPropertyValue(const mozilla::ComputedStyle* computed_values,
                                  const nsAString* name, nsAString* value);

uint32_t Servo_GetCustomPropertiesCount(
    const mozilla::ComputedStyle* computed_values);

bool Servo_GetCustomPropertyNameAt(const mozilla::ComputedStyle*,
                                   uint32_t index, nsAString* name);

void Servo_GetPropertyValue(const mozilla::ComputedStyle* computed_values,
                            nsCSSPropertyID property, nsAString* value);

void Servo_ProcessInvalidations(
    const RawServoStyleSet* set, const mozilla::dom::Element* element,
    const mozilla::ServoElementSnapshotTable* snapshots);

bool Servo_HasPendingRestyleAncestor(const mozilla::dom::Element* element);

void Servo_CssUrlData_GetSerialization(RawServoCssUrlData* url,
                                       uint8_t const** chars, uint32_t* len);

mozilla::URLExtraData* Servo_CssUrlData_GetExtraData(const RawServoCssUrlData*);

bool Servo_CssUrlData_IsLocalRef(const RawServoCssUrlData* url);

// CSS parsing utility functions.

bool Servo_IsValidCSSColor(const nsAString* value);

bool Servo_ComputeColor(const RawServoStyleSet* set, nscolor current_color,
                        const nsAString* value, nscolor* result_color,
                        bool* was_current_color, mozilla::css::Loader* loader);

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
                                    Matrix4x4Components* aResult);

bool Servo_ParseFontShorthandForMatching(
    const nsAString* value, mozilla::URLExtraData* data,
    RefPtr<mozilla::SharedFontList>* family,
    // We use ComputedFontStyleDescriptor just for convenience,
    // but the two values of Oblique are the same.
    mozilla::StyleComputedFontStyleDescriptor* style, float* stretch,
    float* weight);

nsCSSPropertyID Servo_ResolveLogicalProperty(nsCSSPropertyID,
                                             const mozilla::ComputedStyle*);

nsCSSPropertyID Servo_Property_LookupEnabledForAllContent(
    const nsACString* name);

const uint8_t* Servo_Property_GetName(nsCSSPropertyID, uint32_t* out_length);
bool Servo_Property_IsShorthand(const nsACString* name, bool* found);
bool Servo_Property_IsInherited(const nsACString* name);

bool Servo_Property_SupportsType(const nsACString* name, uint8_t ty,
                                 bool* found);

void Servo_Property_GetCSSValuesForProperty(const nsACString* name, bool* found,
                                            nsTArray<nsString>* result);

uint64_t Servo_PseudoClass_GetStates(const nsACString* name);

StyleUseCounters* Servo_UseCounters_Create();
void Servo_UseCounters_Drop(StyleUseCountersOwned);

void Servo_UseCounters_Merge(const StyleUseCounters* doc_counters,
                             const StyleUseCounters* sheet_counters);

bool Servo_IsCssPropertyRecordedInUseCounter(const StyleUseCounters*,
                                             const nsACString* property,
                                             bool* out_known_prop);

RawServoQuotesStrong Servo_Quotes_GetInitialValue();
bool Servo_Quotes_Equal(const RawServoQuotes* a, RawServoQuotes* b);

void Servo_Quotes_GetQuote(const RawServoQuotes* quotes, int32_t depth,
                           mozilla::StyleContentType quote_type,
                           nsAString* result);

// AddRef / Release functions
#define SERVO_ARC_TYPE(name_, type_)         \
  void Servo_##name_##_AddRef(const type_*); \
  void Servo_##name_##_Release(const type_*);
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

}  // extern "C"

#endif  // mozilla_ServoBindings_h
