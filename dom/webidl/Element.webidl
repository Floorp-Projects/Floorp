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

interface Element : Node {
  readonly attribute DOMString? namespaceURI;
  readonly attribute DOMString? prefix;
  readonly attribute DOMString localName;
  readonly attribute DOMString tagName;

           attribute DOMString id;
           attribute DOMString className;
  readonly attribute DOMTokenList classList;

  readonly attribute Attr[] attributes;
  DOMString? getAttribute(DOMString name);
  DOMString? getAttributeNS(DOMString? namespace, DOMString localName);
  void setAttribute(DOMString name, DOMString value);
  void setAttributeNS(DOMString? namespace, DOMString name, DOMString value);
  void removeAttribute(DOMString name);
  void removeAttributeNS(DOMString? namespace, DOMString localName);
  boolean hasAttribute(DOMString name);
  boolean hasAttributeNS(DOMString? namespace, DOMString localName);

  NodeList getElementsByTagName(DOMString qualifiedName);
  NodeList getElementsByTagNameNS(DOMString? namespace, DOMString localName);
  NodeList getElementsByClassName(DOMString classNames);

  readonly attribute HTMLCollection children;
  readonly attribute Element? firstElementChild;
  readonly attribute Element? lastElementChild;
  readonly attribute Element? previousElementSibling;
  readonly attribute Element? nextElementSibling;
  readonly attribute unsigned long childElementCount;

  // NEW
  void prepend((Node or DOMString)... nodes);
  void append((Node or DOMString)... nodes);
  void before((Node or DOMString)... nodes);
  void after((Node or DOMString)... nodes);
  void replace((Node or DOMString)... nodes);
  void remove();
};
