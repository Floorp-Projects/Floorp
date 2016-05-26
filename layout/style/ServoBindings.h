/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindings_h
#define mozilla_ServoBindings_h

#include "stdint.h"
#include "nsColor.h"
#include "mozilla/css/SheetParsingMode.h"
#include "nsProxyRelease.h"

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
namespace mozilla { namespace dom { class Element; } }
using mozilla::dom::Element;
typedef mozilla::dom::Element RawGeckoElement;
class nsIDocument;
typedef nsIDocument RawGeckoDocument;
struct ServoNodeData;
struct ServoComputedValues;
struct RawServoStyleSheet;
struct RawServoStyleSet;
struct nsStyleList;
struct nsStyleImage;
struct nsStyleGradientStop;
class nsStyleGradient;
class nsStyleCoord;
struct nsStyleDisplay;

#define NS_DECL_THREADSAFE_FFI_REFCOUNTING(class_, name_)                     \
  static_assert(class_::HasThreadSafeRefCnt::value,                           \
                "NS_DECL_THREADSAFE_FFI_REFCOUNTING can only be used with "   \
                "classes that have thread-safe refcounting");                 \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr);                    \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr);
#define NS_IMPL_THREADSAFE_FFI_REFCOUNTING(class_, name_)                     \
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

// Gets the class or class list (if any) of the Element.
//
// The calling convention here is rather hairy, and is optimized for getting
// Servo the information it needs for hot calls.
//
// The return value indicates the number of classes. If zero, neither outparam
// is valid. If one, the class_ outparam is filled with the atom of the class.
// If two or more, the classList outparam is set to point to an array of atoms
// representing the class list.
//
// The array is borrowed and the atoms are not addrefed. These values can be
// invalidated by any DOM mutation. Use them in a tight scope.
uint32_t Gecko_ClassOrClassList(RawGeckoElement* element,
                                nsIAtom** class_, nsIAtom*** classList);

// Node data.
ServoNodeData* Gecko_GetNodeData(RawGeckoNode* node);
void Gecko_SetNodeData(RawGeckoNode* node, ServoNodeData* data);
void Servo_DropNodeData(ServoNodeData* data);

// Atoms.
nsIAtom* Gecko_Atomize(const char* aString, uint32_t aLength);
void Gecko_AddRefAtom(nsIAtom* aAtom);
void Gecko_ReleaseAtom(nsIAtom* aAtom);
uint32_t Gecko_HashAtom(nsIAtom* aAtom);
const uint16_t* Gecko_GetAtomAsUTF16(nsIAtom* aAtom, uint32_t* aLength);
bool Gecko_AtomEqualsUTF8(nsIAtom* aAtom, const char* aString, uint32_t aLength);
bool Gecko_AtomEqualsUTF8IgnoreCase(nsIAtom* aAtom, const char* aString, uint32_t aLength);

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

void Gecko_SetGradientStop(nsStyleGradient* gradient,
                           uint32_t index,
                           const nsStyleCoord* location,
                           nscolor color,
                           bool is_interpolation_hint);

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

// Styleset and Stylesheet management.
//
// TODO: Make these return already_AddRefed and UniquePtr when the binding
// generator is smart enough to handle them.
RawServoStyleSheet* Servo_StylesheetFromUTF8Bytes(
    const uint8_t* bytes, uint32_t length,
    mozilla::css::SheetParsingMode parsing_mode,
    ThreadSafeURIHolder* base,
    ThreadSafeURIHolder* referrer,
    ThreadSafePrincipalHolder* principal);
void Servo_AddRefStyleSheet(RawServoStyleSheet* sheet);
void Servo_ReleaseStyleSheet(RawServoStyleSheet* sheet);
void Servo_AppendStyleSheet(RawServoStyleSheet* sheet, RawServoStyleSet* set);
void Servo_PrependStyleSheet(RawServoStyleSheet* sheet, RawServoStyleSet* set);
void Servo_RemoveStyleSheet(RawServoStyleSheet* sheet, RawServoStyleSet* set);
void Servo_InsertStyleSheetBefore(RawServoStyleSheet* sheet,
                                  RawServoStyleSheet* reference,
                                  RawServoStyleSet* set);
bool Servo_StyleSheetHasRules(RawServoStyleSheet* sheet);
RawServoStyleSet* Servo_InitStyleSet();
void Servo_DropStyleSet(RawServoStyleSet* set);

// Computed style data.
ServoComputedValues* Servo_GetComputedValues(RawGeckoNode* node);
ServoComputedValues* Servo_GetComputedValuesForAnonymousBox(ServoComputedValues* parentStyleOrNull,
                                                            nsIAtom* pseudoTag,
                                                            RawServoStyleSet* set);
ServoComputedValues* Servo_GetComputedValuesForPseudoElement(ServoComputedValues* parent_style,
                                                             RawGeckoElement* match_element,
                                                             nsIAtom* pseudo_tag,
                                                             RawServoStyleSet* set,
                                                             bool is_probe);
ServoComputedValues* Servo_InheritComputedValues(ServoComputedValues* parent_style);
void Servo_AddRefComputedValues(ServoComputedValues*);
void Servo_ReleaseComputedValues(ServoComputedValues*);

// Initialize Servo components. Should be called exactly once at startup.
void Servo_Initialize();

// Restyle the given document or subtree.
void Servo_RestyleDocument(RawGeckoDocument* doc, RawServoStyleSet* set);
void Servo_RestyleSubtree(RawGeckoNode* node, RawServoStyleSet* set);

// Style-struct management.
#define STYLE_STRUCT(name, checkdata_cb) \
struct nsStyle##name; \
void Gecko_Construct_nsStyle##name(nsStyle##name* ptr); \
void Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr, const nsStyle##name* other); \
void Gecko_Destroy_nsStyle##name(nsStyle##name* ptr); \
const nsStyle##name* Servo_GetStyle##name(ServoComputedValues* computedValues);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

} // extern "C"

#endif // mozilla_ServoBindings_h
