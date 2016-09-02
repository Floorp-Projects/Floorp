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

namespace mozilla {
namespace dom {
class StyleChildrenIterator;
}
}

using mozilla::dom::StyleChildrenIterator;

// We have these helper types so that we can directly generate
// things like &T or Borrowed<T> on the Rust side in the function, providing
// additional safety benefits.
//
// FFI has a problem with templated types, so we just use raw pointers here.
//
// The "Borrowed" types generate &T or Borrowed<T> in the nullable case.
//
// The "Owned" types generate Owned<T> or OwnedOrNull<T>. Some of these
// are Servo-managed and can be converted to Box<ServoType> on the
// Servo side.
//
// The "Arc" types are Servo-managed Arc<ServoType>s, which are passed
// over FFI as Strong<T> (which is nullable).
// Note that T != ServoType, rather T is ArcInner<ServoType>
#define DECL_BORROWED_REF_TYPE_FOR(type_) typedef type_* type_##Borrowed;
#define DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedOrNull;
#define DECL_BORROWED_MUT_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedMut;
#define DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedMutOrNull;

#define DECL_ARC_REF_TYPE_FOR(type_)         \
  DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_) \
  DECL_BORROWED_REF_TYPE_FOR(type_)          \
  struct MOZ_MUST_USE_TYPE type_##Strong     \
  {                                          \
    type_* mPtr;                             \
    already_AddRefed<type_> Consume();       \
  };

#define DECL_OWNED_REF_TYPE_FOR(type_)    \
  typedef type_* type_##Owned;            \
  DECL_BORROWED_REF_TYPE_FOR(type_)       \
  DECL_BORROWED_MUT_REF_TYPE_FOR(type_)

#define DECL_NULLABLE_OWNED_REF_TYPE_FOR(type_)    \
  typedef type_* type_##OwnedOrNull;               \
  DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_)       \
  DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR(type_)

DECL_ARC_REF_TYPE_FOR(ServoComputedValues)
DECL_ARC_REF_TYPE_FOR(RawServoStyleSheet)
DECL_ARC_REF_TYPE_FOR(ServoDeclarationBlock)

DECL_OWNED_REF_TYPE_FOR(RawServoStyleSet)
DECL_NULLABLE_OWNED_REF_TYPE_FOR(ServoNodeData)
DECL_OWNED_REF_TYPE_FOR(ServoNodeData)
DECL_NULLABLE_OWNED_REF_TYPE_FOR(StyleChildrenIterator)
DECL_OWNED_REF_TYPE_FOR(StyleChildrenIterator)

// We don't use BorrowedMut because the nodes may alias
// Servo itself doesn't directly read or mutate these;
// it only asks Gecko to do so. In case we wish to in
// the future, we should ensure that things being mutated
// are protected from noalias violations by a cell type
DECL_BORROWED_REF_TYPE_FOR(RawGeckoNode)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoNode)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoElement)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoElement)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoDocument)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoDocument)
DECL_BORROWED_MUT_REF_TYPE_FOR(StyleChildrenIterator)

#undef DECL_ARC_REF_TYPE_FOR
#undef DECL_OWNED_REF_TYPE_FOR
#undef DECL_NULLABLE_OWNED_REF_TYPE_FOR
#undef DECL_BORROWED_REF_TYPE_FOR
#undef DECL_NULLABLE_BORROWED_REF_TYPE_FOR
#undef DECL_BORROWED_MUT_REF_TYPE_FOR
#undef DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR


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
uint32_t Gecko_ChildrenCount(RawGeckoNodeBorrowed node);
bool Gecko_NodeIsElement(RawGeckoNodeBorrowed node);
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

// By default, Servo walks the DOM by traversing the siblings of the DOM-view
// first child. This generally works, but misses anonymous children, which we
// want to traverse during styling. To support these cases, we create an
// optional heap-allocated iterator for nodes that need it. If the creation
// method returns null, Servo falls back to the aforementioned simpler (and
// faster) sibling traversal.
StyleChildrenIteratorOwnedOrNull Gecko_MaybeCreateStyleChildrenIterator(RawGeckoNodeBorrowed node);
void Gecko_DropStyleChildrenIterator(StyleChildrenIteratorOwned it);
RawGeckoNodeBorrowedOrNull Gecko_GetNextStyleChild(StyleChildrenIteratorBorrowed it);

// Selector Matching.
uint8_t Gecko_ElementState(RawGeckoElementBorrowed element);
bool Gecko_IsHTMLElementInHTMLDocument(RawGeckoElementBorrowed element);
bool Gecko_IsLink(RawGeckoElementBorrowed element);
bool Gecko_IsTextNode(RawGeckoNodeBorrowed node);
bool Gecko_IsVisitedLink(RawGeckoElementBorrowed element);
bool Gecko_IsUnvisitedLink(RawGeckoElementBorrowed element);
bool Gecko_IsRootElement(RawGeckoElementBorrowed element);
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
                                              ServoElementSnapshot*)

#undef SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS

// Style attributes.
ServoDeclarationBlockBorrowedOrNull Gecko_GetServoDeclarationBlock(RawGeckoElementBorrowed element);

// Node data.
ServoNodeDataBorrowedOrNull Gecko_GetNodeData(RawGeckoNodeBorrowed node);
void Gecko_SetNodeData(RawGeckoNodeBorrowed node, ServoNodeDataOwned data);

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
uint32_t Gecko_GetNodeFlags(RawGeckoNodeBorrowed node);
void Gecko_SetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);
void Gecko_UnsetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);

// Incremental restyle.
// TODO: We would avoid a few ffi calls if we decide to make an API like the
// former CalcAndStoreStyleDifference, but that would effectively mean breaking
// some safety guarantees in the servo side.
//
// Also, we might want a ComputedValues to ComputedValues API for animations?
// Not if we do them in Gecko...
nsStyleContext* Gecko_GetStyleContext(RawGeckoNodeBorrowed node,
                                      nsIAtom* aPseudoTagOrNull);
nsChangeHint Gecko_CalcStyleDifference(nsStyleContext* oldstyle,
                                       ServoComputedValuesBorrowed newstyle);
void Gecko_StoreStyleDifference(RawGeckoNodeBorrowed node, nsChangeHint change);

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

void Gecko_FillAllBackgroundLists(nsStyleImageLayers* layers, uint32_t max_len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

nsCSSShadowArray* Gecko_NewCSSShadowArray(uint32_t len);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSShadowArray, CSSShadowArray);

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
