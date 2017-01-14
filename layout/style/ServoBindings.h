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
  class ServoStyleSheet;
  class FontFamilyList;
  enum FontFamilyType : uint32_t;
}
using mozilla::FontFamilyList;
using mozilla::FontFamilyType;
using mozilla::ServoElementSnapshot;
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

#define NS_DECL_HOLDER_FFI_REFCOUNTING(class_, name_)                         \
  typedef nsMainThreadPtrHolder<class_> ThreadSafe##name_##Holder;            \
  void Gecko_AddRef##name_##ArbitraryThread(ThreadSafe##name_##Holder* aPtr); \
  void Gecko_Release##name_##ArbitraryThread(ThreadSafe##name_##Holder* aPtr);
#define NS_IMPL_HOLDER_FFI_REFCOUNTING(class_, name_)                         \
  void Gecko_AddRef##name_##ArbitraryThread(ThreadSafe##name_##Holder* aPtr)  \
  { NS_ADDREF(aPtr); }                                                        \
  void Gecko_Release##name_##ArbitraryThread(ThreadSafe##name_##Holder* aPtr) \
  { NS_RELEASE(aPtr); }                                                       \


#define DEFINE_ARRAY_TYPE_FOR(type_)                                \
  struct nsTArrayBorrowed_##type_ {                                 \
    nsTArray<type_>* mArray;                                        \
    MOZ_IMPLICIT nsTArrayBorrowed_##type_(nsTArray<type_>* aArray)  \
      : mArray(aArray) {}                                           \
  }
DEFINE_ARRAY_TYPE_FOR(uintptr_t);
#undef DEFINE_ARRAY_TYPE_FOR

extern "C" {

// Object refcounting.
NS_DECL_HOLDER_FFI_REFCOUNTING(nsIPrincipal, Principal)
NS_DECL_HOLDER_FFI_REFCOUNTING(nsIURI, URI)

// DOM Traversal.
uint32_t Gecko_ChildrenCount(RawGeckoNodeBorrowed node);
bool Gecko_NodeIsElement(RawGeckoNodeBorrowed node);
bool Gecko_IsInDocument(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetParentNode(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetFirstChild(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetLastChild(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetPrevSibling(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetNextSibling(RawGeckoNodeBorrowed node);
RawGeckoElementBorrowedOrNull Gecko_GetParentElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetFirstChildElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetLastChildElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetPrevSiblingElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetNextSiblingElement(RawGeckoElementBorrowed element);
RawGeckoElementBorrowedOrNull Gecko_GetDocumentElement(RawGeckoDocumentBorrowed document);
void Gecko_LoadStyleSheet(mozilla::css::Loader* loader,
                          mozilla::ServoStyleSheet* parent,
                          RawServoImportRuleBorrowed import_rule,
                          const uint8_t* url_bytes,
                          uint32_t url_length,
                          const uint8_t* media_bytes,
                          uint32_t media_length);

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
uint16_t Gecko_ElementState(RawGeckoElementBorrowed element);
bool Gecko_IsHTMLElementInHTMLDocument(RawGeckoElementBorrowed element);
bool Gecko_IsLink(RawGeckoElementBorrowed element);
bool Gecko_IsTextNode(RawGeckoNodeBorrowed node);
bool Gecko_IsVisitedLink(RawGeckoElementBorrowed element);
bool Gecko_IsUnvisitedLink(RawGeckoElementBorrowed element);
bool Gecko_IsRootElement(RawGeckoElementBorrowed element);
bool Gecko_MatchesElement(mozilla::CSSPseudoClassType type, RawGeckoElementBorrowed element);
nsIAtom* Gecko_LocalName(RawGeckoElementBorrowed element);
nsIAtom* Gecko_Namespace(RawGeckoElementBorrowed element);
nsIAtom* Gecko_GetElementId(RawGeckoElementBorrowed element);

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
Gecko_GetServoDeclarationBlock(RawGeckoElementBorrowed element);

// Atoms.
nsIAtom* Gecko_Atomize(const char* aString, uint32_t aLength);
void Gecko_AddRefAtom(nsIAtom* aAtom);
void Gecko_ReleaseAtom(nsIAtom* aAtom);
const uint16_t* Gecko_GetAtomAsUTF16(nsIAtom* aAtom, uint32_t* aLength);
bool Gecko_AtomEqualsUTF8(nsIAtom* aAtom, const char* aString, uint32_t aLength);
bool Gecko_AtomEqualsUTF8IgnoreCase(nsIAtom* aAtom, const char* aString, uint32_t aLength);

// Font style
void Gecko_FontFamilyList_Clear(FontFamilyList* aList);
void Gecko_FontFamilyList_AppendNamed(FontFamilyList* aList, nsIAtom* aName);
void Gecko_FontFamilyList_AppendGeneric(FontFamilyList* list, FontFamilyType familyType);
void Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src);

// Counter style.
void Gecko_SetListStyleType(nsStyleList* style_struct, uint32_t type);
void Gecko_CopyListStyleTypeFrom(nsStyleList* dst, const nsStyleList* src);

// background-image style.
// TODO: support element() and -moz-image()
void Gecko_SetNullImageValue(nsStyleImage* image);
void Gecko_SetGradientImageValue(nsStyleImage* image, nsStyleGradient* gradient);
void Gecko_SetUrlImageValue(nsStyleImage* image,
                            const uint8_t* url_bytes,
                            uint32_t url_length,
                            ThreadSafeURIHolder* base_uri,
                            ThreadSafeURIHolder* referrer,
                            ThreadSafePrincipalHolder* principal);
void Gecko_CopyImageValueFrom(nsStyleImage* image, const nsStyleImage* other);

nsStyleGradient* Gecko_CreateGradient(uint8_t shape,
                                      uint8_t size,
                                      bool repeating,
                                      bool legacy_syntax,
                                      uint32_t stops);

// list-style-image style.
void Gecko_SetListStyleImageNone(nsStyleList* style_struct);
void Gecko_SetListStyleImage(nsStyleList* style_struct,
                             const uint8_t* string_bytes, uint32_t string_length,
                             ThreadSafeURIHolder* base_uri,
                             ThreadSafeURIHolder* referrer,
                             ThreadSafePrincipalHolder* principal);
void Gecko_CopyListStyleImageFrom(nsStyleList* dest, const nsStyleList* src);

// cursor style.
void Gecko_SetCursorArrayLength(nsStyleUserInterface* ui, size_t len);
void Gecko_SetCursorImage(nsCursorImage* cursor,
                          const uint8_t* string_bytes, uint32_t string_length,
                          ThreadSafeURIHolder* base_uri,
                          ThreadSafeURIHolder* referrer,
                          ThreadSafePrincipalHolder* principal);
void Gecko_CopyCursorArrayFrom(nsStyleUserInterface* dest,
                               const nsStyleUserInterface* src);

// Display style.
void Gecko_SetMozBinding(nsStyleDisplay* style_struct,
                         const uint8_t* string_bytes, uint32_t string_length,
                         ThreadSafeURIHolder* base_uri,
                         ThreadSafeURIHolder* referrer,
                         ThreadSafePrincipalHolder* principal);
void Gecko_CopyMozBindingFrom(nsStyleDisplay* des, const nsStyleDisplay* src);

// Dirtiness tracking.
uint32_t Gecko_GetNodeFlags(RawGeckoNodeBorrowed node);
void Gecko_SetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);
void Gecko_UnsetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);

// Incremental restyle.
// Also, we might want a ComputedValues to ComputedValues API for animations?
// Not if we do them in Gecko...
nsStyleContext* Gecko_GetStyleContext(RawGeckoNodeBorrowed node,
                                      nsIAtom* aPseudoTagOrNull);
nsChangeHint Gecko_CalcStyleDifference(nsStyleContext* oldstyle,
                                       ServoComputedValuesBorrowed newstyle);

// Element snapshot.
ServoElementSnapshotOwned Gecko_CreateElementSnapshot(RawGeckoElementBorrowed element);
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

// Clear the mContents field in nsStyleContent. This is needed to run the
// destructors, otherwise we'd leak the images (though we still don't support
// those), strings, and whatnot.
void Gecko_ClearStyleContents(nsStyleContent* content);
void Gecko_CopyStyleContentsFrom(nsStyleContent* content, const nsStyleContent* other);

void Gecko_EnsureImageLayersLength(nsStyleImageLayers* layers, size_t len,
                                   nsStyleImageLayers::LayerType layer_type);

void Gecko_EnsureStyleAnimationArrayLength(void* array, size_t len);

// Clean up pointer-based coordinates
void Gecko_ResetStyleCoord(nsStyleUnit* unit, nsStyleUnion* value);

// Set an nsStyleCoord to a computed `calc()` value
void Gecko_SetStyleCoordCalcValue(nsStyleUnit* unit, nsStyleUnion* value, nsStyleCoord::CalcValue calc);

void Gecko_CopyClipPathValueFrom(mozilla::StyleClipPath* dst, const mozilla::StyleClipPath* src);

void Gecko_DestroyClipPath(mozilla::StyleClipPath* clip);
mozilla::StyleBasicShape* Gecko_NewBasicShape(mozilla::StyleBasicShapeType type);

void Gecko_ResetFilters(nsStyleEffects* effects, size_t new_len);
void Gecko_CopyFiltersFrom(nsStyleEffects* aSrc, nsStyleEffects* aDest);

void Gecko_FillAllBackgroundLists(nsStyleImageLayers* layers, uint32_t max_len);
void Gecko_FillAllMaskLists(nsStyleImageLayers* layers, uint32_t max_len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

nsCSSShadowArray* Gecko_NewCSSShadowArray(uint32_t len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSShadowArray, CSSShadowArray);

nsStyleQuoteValues* Gecko_NewStyleQuoteValues(uint32_t len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleQuoteValues, QuoteValues);

nsCSSValueSharedList* Gecko_NewCSSValueSharedList(uint32_t len);
void Gecko_CSSValue_SetAbsoluteLength(nsCSSValueBorrowedMut css_value, nscoord len);
void Gecko_CSSValue_SetNumber(nsCSSValueBorrowedMut css_value, float number);
void Gecko_CSSValue_SetKeyword(nsCSSValueBorrowedMut css_value, nsCSSKeyword keyword);
void Gecko_CSSValue_SetPercentage(nsCSSValueBorrowedMut css_value, float percent);
void Gecko_CSSValue_SetAngle(nsCSSValueBorrowedMut css_value, float radians);
void Gecko_CSSValue_SetCalc(nsCSSValueBorrowedMut css_value, nsStyleCoord::CalcValue calc);
void Gecko_CSSValue_SetFunction(nsCSSValueBorrowedMut css_value, int32_t len);
nsCSSValueBorrowedMut Gecko_CSSValue_GetArrayItem(nsCSSValueBorrowedMut css_value, int32_t index);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSValueSharedList, CSSValueSharedList);
bool Gecko_PropertyId_IsPrefEnabled(nsCSSPropertyID id);

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

#define SERVO_BINDING_FUNC(name_, return_, ...) return_ name_(__VA_ARGS__);
#include "mozilla/ServoBindingList.h"
#undef SERVO_BINDING_FUNC

} // extern "C"

#endif // mozilla_ServoBindings_h
