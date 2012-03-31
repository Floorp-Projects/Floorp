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

interface Document : Node {
  readonly attribute DOMImplementation implementation;
  readonly attribute DOMString URL;
  readonly attribute DOMString documentURI;
  readonly attribute DOMString compatMode;
  readonly attribute DOMString characterSet;
  readonly attribute DOMString contentType;

  readonly attribute DocumentType? doctype;
  readonly attribute Element? documentElement;
  NodeList getElementsByTagName(DOMString qualifiedName);
  NodeList getElementsByTagNameNS(DOMString? namespace, DOMString localName);
  NodeList getElementsByClassName(DOMString classNames);
  Element? getElementById(DOMString elementId);

  Element createElement(DOMString localName);
  Element createElementNS(DOMString? namespace, DOMString qualifiedName);
  DocumentFragment createDocumentFragment();
  Text createTextNode(DOMString data);
  Comment createComment(DOMString data);
  ProcessingInstruction createProcessingInstruction(DOMString target, DOMString data);

  Node importNode(Node node, optional boolean deep);
  Node adoptNode(Node node);

  Event createEvent(DOMString eventInterfaceName);

  Range createRange();

  NodeIterator createNodeIterator(Node root, optional unsigned long whatToShow, optional NodeFilter? filter);
  TreeWalker createTreeWalker(Node root, optional unsigned long whatToShow, optional NodeFilter? filter);

  // NEW
  void prepend((Node or DOMString)... nodes);
  void append((Node or DOMString)... nodes);
};

interface XMLDocument : Document {};
