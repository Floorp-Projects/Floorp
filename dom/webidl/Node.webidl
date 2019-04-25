/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-dom-20120105/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Principal;
interface URI;

interface Node : EventTarget {
  const unsigned short ELEMENT_NODE = 1;
  const unsigned short ATTRIBUTE_NODE = 2; // historical
  const unsigned short TEXT_NODE = 3;
  const unsigned short CDATA_SECTION_NODE = 4; // historical
  const unsigned short ENTITY_REFERENCE_NODE = 5; // historical
  const unsigned short ENTITY_NODE = 6; // historical
  const unsigned short PROCESSING_INSTRUCTION_NODE = 7;
  const unsigned short COMMENT_NODE = 8;
  const unsigned short DOCUMENT_NODE = 9;
  const unsigned short DOCUMENT_TYPE_NODE = 10;
  const unsigned short DOCUMENT_FRAGMENT_NODE = 11;
  const unsigned short NOTATION_NODE = 12; // historical
  [Constant]
  readonly attribute unsigned short nodeType;
  [Pure]
  readonly attribute DOMString nodeName;

  [Pure, Throws, NeedsCallerType, BinaryName="baseURIFromJS"]
  readonly attribute DOMString? baseURI;

  [Pure, BinaryName=isInComposedDoc]
  readonly attribute boolean isConnected;
  [Pure]
  readonly attribute Document? ownerDocument;
  [Pure]
  Node getRootNode(optional GetRootNodeOptions options);
  [Pure]
  readonly attribute Node? parentNode;
  [Pure]
  readonly attribute Element? parentElement;
  [Pure]
  boolean hasChildNodes();
  [SameObject]
  readonly attribute NodeList childNodes;
  [Pure]
  readonly attribute Node? firstChild;
  [Pure]
  readonly attribute Node? lastChild;
  [Pure]
  readonly attribute Node? previousSibling;
  [Pure]
  readonly attribute Node? nextSibling;

  [CEReactions, SetterThrows, Pure]
           attribute DOMString? nodeValue;
  [CEReactions, SetterThrows, GetterCanOOM,
   SetterNeedsSubjectPrincipal=NonSystem, Pure]
           attribute DOMString? textContent;
  // These DOM methods cannot be accessed by UA Widget scripts
  // because the DOM element reflectors will be in the content scope,
  // instead of the desired UA Widget scope.
  [CEReactions, Throws, Func="IsNotUAWidget"]
  Node insertBefore(Node node, Node? child);
  [CEReactions, Throws, Func="IsNotUAWidget"]
  Node appendChild(Node node);
  [CEReactions, Throws, Func="IsNotUAWidget"]
  Node replaceChild(Node node, Node child);
  [CEReactions, Throws]
  Node removeChild(Node child);
  [CEReactions]
  void normalize();

  [CEReactions, Throws, Func="IsNotUAWidget"]
  Node cloneNode(optional boolean deep = false);
  [Pure]
  boolean isSameNode(Node? node);
  [Pure]
  boolean isEqualNode(Node? node);

  const unsigned short DOCUMENT_POSITION_DISCONNECTED = 0x01;
  const unsigned short DOCUMENT_POSITION_PRECEDING = 0x02;
  const unsigned short DOCUMENT_POSITION_FOLLOWING = 0x04;
  const unsigned short DOCUMENT_POSITION_CONTAINS = 0x08;
  const unsigned short DOCUMENT_POSITION_CONTAINED_BY = 0x10;
  const unsigned short DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 0x20; // historical
  [Pure]
  unsigned short compareDocumentPosition(Node other);
  [Pure]
  boolean contains(Node? other);

  [Pure]
  DOMString? lookupPrefix(DOMString? namespace);
  [Pure]
  DOMString? lookupNamespaceURI(DOMString? prefix);
  [Pure]
  boolean isDefaultNamespace(DOMString? namespace);

  // Mozilla-specific stuff
  [ChromeOnly]
  readonly attribute Principal nodePrincipal;
  [ChromeOnly]
  readonly attribute URI? baseURIObject;
  [ChromeOnly]
  DOMString generateXPath();
  [ChromeOnly, Pure, BinaryName="flattenedTreeParentNodeNonInline"]
  readonly attribute Node? flattenedTreeParentNode;

  // Mozilla devtools-specific stuff
  /**
   * If this element is a flex item (or has one or more anonymous box ancestors
   * that chain up to an anonymous flex item), then this method returns the
   * flex container that the flex item participates in. Otherwise, this method
   * returns null.
   */
  [ChromeOnly]
  readonly attribute Element? parentFlexElement;

#ifdef ACCESSIBILITY
  [Func="mozilla::dom::AccessibleNode::IsAOMEnabled", SameObject]
  readonly attribute AccessibleNode? accessibleNode;
#endif
};

dictionary GetRootNodeOptions {
  boolean composed = false;
};
