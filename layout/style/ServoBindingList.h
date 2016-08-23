/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all Servo binding functions */

/* This file contains the list of all Servo binding functions. Each
 * entry is defined as a SERVO_BINDING_FUNC macro with the following
 * parameters:
 * - 'name_' the name of the binding function
 * - 'return_' the return type of the binding function
 * and the parameter list of the function.
 *
 * Users of this list should define a macro
 * SERVO_BINDING_FUNC(name_, return_, ...)
 * before including this file.
 */

// Node data
SERVO_BINDING_FUNC(Servo_DropNodeData, void, ServoNodeData* data)

// Styleset and Stylesheet management
SERVO_BINDING_FUNC(Servo_StylesheetFromUTF8Bytes, RawServoStyleSheetStrong,
                   const uint8_t* bytes, uint32_t length,
                   mozilla::css::SheetParsingMode parsing_mode,
                   const uint8_t* base_bytes, uint32_t base_length,
                   ThreadSafeURIHolder* base,
                   ThreadSafeURIHolder* referrer,
                   ThreadSafePrincipalHolder* principal)
SERVO_BINDING_FUNC(Servo_AddRefStyleSheet, void,
                   RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_ReleaseStyleSheet, void,
                   RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_AppendStyleSheet, void,
                   RawServoStyleSheetBorrowed sheet, RawServoStyleSet* set)
SERVO_BINDING_FUNC(Servo_PrependStyleSheet, void,
                   RawServoStyleSheetBorrowed sheet, RawServoStyleSet* set)
SERVO_BINDING_FUNC(Servo_RemoveStyleSheet, void,
                   RawServoStyleSheetBorrowed sheet, RawServoStyleSet* set)
SERVO_BINDING_FUNC(Servo_InsertStyleSheetBefore, void,
                   RawServoStyleSheetBorrowed sheet,
                   RawServoStyleSheetBorrowed reference,
                   RawServoStyleSet* set)
SERVO_BINDING_FUNC(Servo_StyleSheetHasRules, bool,
                   RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_InitStyleSet, RawServoStyleSet*)
SERVO_BINDING_FUNC(Servo_DropStyleSet, void, RawServoStyleSet* set)

// Style attribute
SERVO_BINDING_FUNC(Servo_ParseStyleAttribute, ServoDeclarationBlockStrong,
                   const uint8_t* bytes, uint32_t length,
                   nsHTMLCSSStyleSheet* cache)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_AddRef, void,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_Release, void,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_GetDeclarationBlockCache, nsHTMLCSSStyleSheet*,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_SetDeclarationBlockImmutable, void,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_ClearDeclarationBlockCachePointer, void,
                   ServoDeclarationBlockBorrowed declarations)

// CSS supports()
SERVO_BINDING_FUNC(Servo_CSSSupports, bool,
                   const uint8_t* name, uint32_t name_length,
                   const uint8_t* value, uint32_t value_length)

// Computed style data
SERVO_BINDING_FUNC(Servo_GetComputedValues, ServoComputedValuesStrong,
                   RawGeckoNode* node)
SERVO_BINDING_FUNC(Servo_GetComputedValuesForAnonymousBox,
                   ServoComputedValuesStrong,
                   ServoComputedValuesBorrowed parent_style_or_null,
                   nsIAtom* pseudoTag, RawServoStyleSet* set)
SERVO_BINDING_FUNC(Servo_GetComputedValuesForPseudoElement,
                   ServoComputedValuesStrong,
                   ServoComputedValuesBorrowed parent_style,
                   RawGeckoElement* match_element, nsIAtom* pseudo_tag,
                   RawServoStyleSet* set, bool is_probe)
SERVO_BINDING_FUNC(Servo_InheritComputedValues, ServoComputedValuesStrong,
                   ServoComputedValuesBorrowed parent_style)
SERVO_BINDING_FUNC(Servo_AddRefComputedValues, void,
                   ServoComputedValuesBorrowed computed_values)
SERVO_BINDING_FUNC(Servo_ReleaseComputedValues, void,
                   ServoComputedValuesBorrowed computed_values)

// Initialize Servo components. Should be called exactly once at startup.
SERVO_BINDING_FUNC(Servo_Initialize, void)
// Shut down Servo components. Should be called exactly once at shutdown.
SERVO_BINDING_FUNC(Servo_Shutdown, void)

// Restyle hints
SERVO_BINDING_FUNC(Servo_ComputeRestyleHint, nsRestyleHint,
                   RawGeckoElement* element, ServoElementSnapshot* snapshot,
                   RawServoStyleSet* set)

  // Restyle the given document or subtree
SERVO_BINDING_FUNC(Servo_RestyleDocument, void,
                   RawGeckoDocument* doc, RawServoStyleSet* set)
SERVO_BINDING_FUNC(Servo_RestyleSubtree, void,
                   RawGeckoNode* node, RawServoStyleSet* set)

// Style-struct management.
#define STYLE_STRUCT(name, checkdata_cb)                            \
  struct nsStyle##name;                                             \
  SERVO_BINDING_FUNC(Servo_GetStyle##name, const nsStyle##name*,  \
                     ServoComputedValuesBorrowed computed_values)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
