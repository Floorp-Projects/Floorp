/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Func="mozilla::dom::AccessibleNode::IsAOMEnabled",
 Exposed=Window]
interface AccessibleNode {
  readonly attribute DOMString computedRole;
  [Frozen, Cached, Pure]
  readonly attribute sequence<DOMString> states;
  [Frozen, Cached, Pure]
  readonly attribute sequence<DOMString> attributes;
  readonly attribute Node? DOMNode;

  boolean is(DOMString... states);
  boolean has(DOMString... attributes);
  [Throws]
  any get(DOMString attribute);

  attribute DOMString? role;
  attribute DOMString? roleDescription;

  // Accessible label and descriptor
  attribute DOMString? label;

  // Global states and properties
  attribute DOMString? current;

  // Accessible properties
  attribute DOMString? autocomplete;
  attribute DOMString? keyShortcuts;
  attribute boolean? modal;
  attribute boolean? multiline;
  attribute boolean? multiselectable;
  attribute DOMString? orientation;
  attribute boolean? readOnly;
  attribute boolean? required;
  attribute DOMString? sort;

  // Range values
  attribute DOMString? placeholder;
  attribute double? valueMax;
  attribute double? valueMin;
  attribute double? valueNow;
  attribute DOMString? valueText;

  // Accessible states
  attribute DOMString? checked;
  attribute boolean? disabled;
  attribute boolean? expanded;
  attribute DOMString? hasPopUp;
  attribute boolean? hidden;
  attribute DOMString? invalid;
  attribute DOMString? pressed;
  attribute boolean? selected;

  // Live regions
  attribute boolean? atomic;
  attribute boolean? busy;
  attribute DOMString? live;
  attribute DOMString? relevant;

  // Other relationships
  attribute AccessibleNode? activeDescendant;
  attribute AccessibleNode? details;
  attribute AccessibleNode? errorMessage;

  // Collections.
  attribute long? colCount;
  attribute unsigned long? colIndex;
  attribute unsigned long? colSpan;
  attribute unsigned long? level;
  attribute unsigned long? posInSet;
  attribute long? rowCount;
  attribute unsigned long? rowIndex;
  attribute unsigned long? rowSpan;
  attribute long? setSize;
};
