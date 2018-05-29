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
};
