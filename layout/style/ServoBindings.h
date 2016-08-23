/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindings_h
#define mozilla_ServoBindings_h

#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/css/SheetParsingMode.h"
#include "nsChangeHint.h"
#include "nsColor.h"
#include "nsProxyRelease.h"
#include "nsStyleCoord.h"
#include "nsStyleStruct.h"
#include "stdint.h"

/*
 * API for Servo to access Gecko data structures. This file must compile as valid
 * C code in order for the binding generator to parse it.
 *
 * Functions beginning with Gecko_ are implemented in Gecko and invoked from Servo.
 * Functions beginning with Servo_ are implemented in Servo and invoked from Gecko.
 */

class nsIAtom;
class nsINode;
typedef nsINode RawGeckoNode;
class nsIPrincipal;
class nsIURI;
struct nsFont;
namespace mozilla {
  class FontFamilyList;
  enum FontFamilyType : uint32_t;
  namespace dom { class Element; }
}
using mozilla::FontFamilyList;
using mozilla::FontFamilyType;
using mozilla::dom::Element;
using mozilla::ServoElementSnapshot;
typedef mozilla::dom::Element RawGeckoElement;
class nsIDocument;
typedef nsIDocument RawGeckoDocument;
struct ServoNodeData;
struct ServoComputedValues;
struct RawServoStyleSheet;
struct RawServoStyleSet;
class nsHTMLCSSStyleSheet;
struct nsStyleList;
struct nsStyleImage;
struct nsStyleGradientStop;
class nsStyleGradient;
class nsStyleCoord;
struct nsStyleDisplay;
struct ServoDeclarationBlock;

#define DECL_REF_TYPE_FOR(type_)          \
  typedef type_* type_##Borrowed;         \
  struct MOZ_MUST_USE_TYPE type_##Strong  \
  {                                       \
    type_* mPtr;                          \
    already_AddRefed<type_> Consume();    \
  };

DECL_REF_TYPE_FOR(ServoComputedValues)
DECL_REF_TYPE_FOR(RawServoStyleSheet)
DECL_REF_TYPE_FOR(ServoDeclarationBlock)

#undef DECL_REF_TYPE_FOR

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

extern "C" {

// DOM Traversal.
uint32_t Gecko_ChildrenCount(RawGeckoNode* node);
bool Gecko_NodeIsElement(RawGeckoNode* node);
RawGeckoNode* Gecko_GetParentNode(RawGeckoNode* node);
RawGeckoNode* Gecko_GetFirstChild(RawGeckoNode* node);
RawGeckoNode* Gecko_GetLastChild(RawGeckoNode* node);
RawGeckoNode* Gecko_GetPrevSibling(RawGeckoNode* node);
RawGeckoNode* Gecko_GetNextSibling(RawGeckoNode* node);
RawGeckoElement* Gecko_GetParentElement(RawGeckoElement* element);
RawGeckoElement* Gecko_GetFirstChildElement(RawGeckoElement* element);
RawGeckoElement* Gecko_GetLastChildElement(RawGeckoElement* element);
RawGeckoElement* Gecko_GetPrevSiblingElement(RawGeckoElement* element);
RawGeckoElement* Gecko_GetNextSiblingElement(RawGeckoElement* element);
RawGeckoElement* Gecko_GetDocumentElement(RawGeckoDocument* document);

// Selector Matching.
uint8_t Gecko_ElementState(RawGeckoElement* element);
bool Gecko_IsHTMLElementInHTMLDocument(RawGeckoElement* element);
bool Gecko_IsLink(RawGeckoElement* element);
bool Gecko_IsTextNode(RawGeckoNode* node);
bool Gecko_IsVisitedLink(RawGeckoElement* element);
bool Gecko_IsUnvisitedLink(RawGeckoElement* element);
bool Gecko_IsRootElement(RawGeckoElement* element);
nsIAtom* Gecko_LocalName(RawGeckoElement* element);
nsIAtom* Gecko_Namespace(RawGeckoElement* element);
nsIAtom* Gecko_GetElementId(RawGeckoElement* element);

// Attributes.
#define SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)   \
  nsIAtom* prefix_##AtomAttrValue(implementor_* element, nsIAtom* attribute);  \
  bool prefix_##HasAttr(implementor_* element, nsIAtom* ns, nsIAtom* name);    \
  bool prefix_##AttrEquals(implementor_* element, nsIAtom* ns, nsIAtom* name,  \
                           nsIAtom* str, bool ignoreCase);                     \
  bool prefix_##AttrDashEquals(implementor_* element, nsIAtom* ns,             \
                               nsIAtom* name, nsIAtom* str);                   \
  bool prefix_##AttrIncludes(implementor_* element, nsIAtom* ns,               \
                             nsIAtom* name, nsIAtom* str);                     \
  bool prefix_##AttrHasSubstring(implementor_* element, nsIAtom* ns,           \
                                 nsIAtom* name, nsIAtom* str);                 \
  bool prefix_##AttrHasPrefix(implementor_* element, nsIAtom* ns,              \
                              nsIAtom* name, nsIAtom* str);                    \
  bool prefix_##AttrHasSuffix(implementor_* element, nsIAtom* ns,              \
                              nsIAtom* name, nsIAtom* str);                    \
  uint32_t prefix_##ClassOrClassList(implementor_* element, nsIAtom** class_,  \
                                     nsIAtom*** classList);

SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_, RawGeckoElement)
SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_Snapshot,
                                              ServoElementSnapshot)

#undef SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS

// Style attributes.
ServoDeclarationBlockBorrowed Gecko_GetServoDeclarationBlock(RawGeckoElement* element);

// Node data.
ServoNodeData* Gecko_GetNodeData(RawGeckoNode* node);
void Gecko_SetNodeData(RawGeckoNode* node, ServoNodeData* data);

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
// TODO: support url() values (and maybe element() too?).
void Gecko_SetNullImageValue(nsStyleImage* image);
void Gecko_SetGradientImageValue(nsStyleImage* image, nsStyleGradient* gradient);
void Gecko_CopyImageValueFrom(nsStyleImage* image, const nsStyleImage* other);

nsStyleGradient* Gecko_CreateGradient(uint8_t shape,
                                      uint8_t size,
                                      bool repeating,
                                      bool legacy_syntax,
                                      uint32_t stops);

// Object refcounting.
NS_DECL_HOLDER_FFI_REFCOUNTING(nsIPrincipal, Principal)
NS_DECL_HOLDER_FFI_REFCOUNTING(nsIURI, URI)

// Display style.
void Gecko_SetMozBinding(nsStyleDisplay* style_struct,
                         const uint8_t* string_bytes, uint32_t string_length,
                         ThreadSafeURIHolder* base_uri,
                         ThreadSafeURIHolder* referrer,
                         ThreadSafePrincipalHolder* principal);
void Gecko_CopyMozBindingFrom(nsStyleDisplay* des, const nsStyleDisplay* src);

// Dirtiness tracking.
uint32_t Gecko_GetNodeFlags(RawGeckoNode* node);
void Gecko_SetNodeFlags(RawGeckoNode* node, uint32_t flags);
void Gecko_UnsetNodeFlags(RawGeckoNode* node, uint32_t flags);

// Incremental restyle.
// TODO: We would avoid a few ffi calls if we decide to make an API like the
// former CalcAndStoreStyleDifference, but that would effectively mean breaking
// some safety guarantees in the servo side.
//
// Also, we might want a ComputedValues to ComputedValues API for animations?
// Not if we do them in Gecko...
nsStyleContext* Gecko_GetStyleContext(RawGeckoNode* node,
                                      nsIAtom* aPseudoTagOrNull);
nsChangeHint Gecko_CalcStyleDifference(nsStyleContext* oldstyle,
                                       ServoComputedValuesBorrowed newstyle);
void Gecko_StoreStyleDifference(RawGeckoNode* node, nsChangeHint change);

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

void Gecko_EnsureImageLayersLength(nsStyleImageLayers* layers, size_t len);

void Gecko_InitializeImageLayer(nsStyleImageLayers::Layer* layer,
                                nsStyleImageLayers::LayerType layer_type);

// Clean up pointer-based coordinates
void Gecko_ResetStyleCoord(nsStyleUnit* unit, nsStyleUnion* value);

// Set an nsStyleCoord to a computed `calc()` value
void Gecko_SetStyleCoordCalcValue(nsStyleUnit* unit, nsStyleUnion* value, nsStyleCoord::CalcValue calc);

void Gecko_CopyClipPathValueFrom(mozilla::StyleClipPath* dst, const mozilla::StyleClipPath* src);

void Gecko_DestroyClipPath(mozilla::StyleClipPath* clip);
mozilla::StyleBasicShape* Gecko_NewBasicShape(mozilla::StyleBasicShapeType type);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

// Style-struct management.
#define STYLE_STRUCT(name, checkdata_cb)                                       \
  struct nsStyle##name;                                                        \
  void Gecko_Construct_nsStyle##name(nsStyle##name* ptr);                      \
  void Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,                   \
                                         const nsStyle##name* other);          \
  void Gecko_Destroy_nsStyle##name(nsStyle##name* ptr);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

#define SERVO_BINDING_FUNC(name_, return_, ...) return_ name_(__VA_ARGS__);
#include "mozilla/ServoBindingList.h"
#undef SERVO_BINDING_FUNC

} // extern "C"

#endif // mozilla_ServoBindings_h
