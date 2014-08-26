/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface NamedNodeMap {
  getter Attr? getNamedItem(DOMString name);
  [Throws]
  Attr? setNamedItem(Attr arg);
  [Throws]
  Attr removeNamedItem(DOMString name);

  getter Attr? item(unsigned long index);
  readonly attribute unsigned long length;

  Attr? getNamedItemNS(DOMString? namespaceURI, DOMString localName);
  [Throws]
  Attr? setNamedItemNS(Attr arg);
  [Throws]
  Attr removeNamedItemNS(DOMString? namespaceURI, DOMString localName);
};
