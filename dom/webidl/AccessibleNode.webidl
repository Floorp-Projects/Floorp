/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Func="mozilla::dom::AccessibleNode::IsAOMEnabled"]
interface AccessibleNode {
  readonly attribute DOMString role;
  [Frozen, Cached, Pure]
  readonly attribute sequence<DOMString> states;
  [Frozen, Cached, Pure]
  readonly attribute sequence<DOMString> attributes;
  readonly attribute Node? DOMNode;

  boolean is(DOMString... states);
  boolean has(DOMString... attributes);
  [Throws]
  any get(DOMString attribute);

  // Accessible properties
  attribute boolean? modal;
  attribute boolean? multiline;
  attribute boolean? multiselectable;
  attribute boolean? readOnly;
  attribute boolean? required;

  // Range values
  attribute double? valueMax;
  attribute double? valueMin;
  attribute double? valueNow;

  // Accessible states
  attribute boolean? disabled;
  attribute boolean? expanded;
  attribute boolean? hidden;
  attribute boolean? selected;

  // Live regions
  attribute boolean? atomic;
  attribute boolean? busy;

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
