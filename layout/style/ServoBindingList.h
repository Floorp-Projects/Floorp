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
SERVO_BINDING_FUNC(Servo_Node_ClearNodeData, void, RawGeckoNode* node)

// Styleset and Stylesheet management
SERVO_BINDING_FUNC(Servo_StyleSheet_FromUTF8Bytes, RawServoStyleSheetStrong,
                   const uint8_t* bytes, uint32_t length,
                   mozilla::css::SheetParsingMode parsing_mode,
                   const uint8_t* base_bytes, uint32_t base_length,
                   ThreadSafeURIHolder* base,
                   ThreadSafeURIHolder* referrer,
                   ThreadSafePrincipalHolder* principal)
SERVO_BINDING_FUNC(Servo_StyleSheet_AddRef, void,
                   RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_StyleSheet_Release, void,
                   RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_StyleSheet_HasRules, bool,
                   RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_StyleSet_Init, RawServoStyleSetOwned)
SERVO_BINDING_FUNC(Servo_StyleSet_Drop, void, RawServoStyleSetOwned set)
SERVO_BINDING_FUNC(Servo_StyleSet_AppendStyleSheet, void,
                   RawServoStyleSetBorrowedMut set, RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_StyleSet_PrependStyleSheet, void,
                   RawServoStyleSetBorrowedMut set, RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_StyleSet_RemoveStyleSheet, void,
                   RawServoStyleSetBorrowedMut set, RawServoStyleSheetBorrowed sheet)
SERVO_BINDING_FUNC(Servo_StyleSet_InsertStyleSheetBefore, void,
                   RawServoStyleSetBorrowedMut set, RawServoStyleSheetBorrowed sheet,
                   RawServoStyleSheetBorrowed reference)

// Style attribute
SERVO_BINDING_FUNC(Servo_ParseStyleAttribute, ServoDeclarationBlockStrong,
                   const uint8_t* bytes, uint32_t length,
                   nsHTMLCSSStyleSheet* cache)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_AddRef, void,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_Release, void,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_GetCache, nsHTMLCSSStyleSheet*,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_SetImmutable, void,
                   ServoDeclarationBlockBorrowed declarations)
SERVO_BINDING_FUNC(Servo_DeclarationBlock_ClearCachePointer, void,
                   ServoDeclarationBlockBorrowed declarations)

// CSS supports()
SERVO_BINDING_FUNC(Servo_CSSSupports, bool,
                   const uint8_t* name, uint32_t name_length,
                   const uint8_t* value, uint32_t value_length)

// Computed style data
SERVO_BINDING_FUNC(Servo_ComputedValues_Get, ServoComputedValuesStrong,
                   RawGeckoNodeBorrowed node)
SERVO_BINDING_FUNC(Servo_ComputedValues_GetForAnonymousBox,
                   ServoComputedValuesStrong,
                   ServoComputedValuesBorrowedOrNull parent_style_or_null,
                   nsIAtom* pseudoTag, RawServoStyleSetBorrowedMut set)
SERVO_BINDING_FUNC(Servo_ComputedValues_GetForPseudoElement,
                   ServoComputedValuesStrong,
                   ServoComputedValuesBorrowed parent_style,
                   RawGeckoElementBorrowed match_element, nsIAtom* pseudo_tag,
                   RawServoStyleSetBorrowedMut set, bool is_probe)
SERVO_BINDING_FUNC(Servo_ComputedValues_Inherit, ServoComputedValuesStrong,
                   ServoComputedValuesBorrowedOrNull parent_style)
SERVO_BINDING_FUNC(Servo_ComputedValues_AddRef, void,
                   ServoComputedValuesBorrowed computed_values)
SERVO_BINDING_FUNC(Servo_ComputedValues_Release, void,
                   ServoComputedValuesBorrowed computed_values)

// Initialize Servo components. Should be called exactly once at startup.
SERVO_BINDING_FUNC(Servo_Initialize, void)
// Shut down Servo components. Should be called exactly once at shutdown.
SERVO_BINDING_FUNC(Servo_Shutdown, void)

// Restyle hints
SERVO_BINDING_FUNC(Servo_ComputeRestyleHint, nsRestyleHint,
                   RawGeckoElement* element, ServoElementSnapshot* snapshot,
                   RawServoStyleSetBorrowed set)

// Restyle the given subtree.
SERVO_BINDING_FUNC(Servo_RestyleSubtree, void,
                   RawGeckoNodeBorrowed node, RawServoStyleSetBorrowedMut set)

// Style-struct management.
#define STYLE_STRUCT(name, checkdata_cb)                            \
  struct nsStyle##name;                                             \
  SERVO_BINDING_FUNC(Servo_GetStyle##name, const nsStyle##name*,  \
                     ServoComputedValuesBorrowed computed_values)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
