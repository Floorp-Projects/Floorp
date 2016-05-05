/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindings_h
#define mozilla_ServoBindings_h

#include "stdint.h"
#include "mozilla/css/SheetParsingMode.h"

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
namespace mozilla { namespace dom { class Element; } }
using mozilla::dom::Element;
typedef mozilla::dom::Element RawGeckoElement;
class nsIDocument;
typedef nsIDocument RawGeckoDocument;
struct ServoNodeData;
struct ServoComputedValues;
struct RawServoStyleSheet;
struct RawServoStyleSet;

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

// Node data.
ServoNodeData* Gecko_GetNodeData(RawGeckoNode* node);
void Gecko_SetNodeData(RawGeckoNode* node, ServoNodeData* data);
void Servo_DropNodeData(ServoNodeData* data);

// Styleset and Stylesheet management.
//
// TODO: Make these return already_AddRefed and UniquePtr when the binding
// generator is smart enough to handle them.
RawServoStyleSheet* Servo_StylesheetFromUTF8Bytes(const uint8_t* bytes, uint32_t length,
                                                  mozilla::css::SheetParsingMode parsing_mode);
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
void Servo_AddRefComputedValues(ServoComputedValues*);
void Servo_ReleaseComputedValues(ServoComputedValues*);

// Initialize Servo components. Should be called exactly once at startup.
void Servo_Initialize();

// Restyle the given document.
void Servo_RestyleDocument(RawGeckoDocument* doc, RawServoStyleSet* set);

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
