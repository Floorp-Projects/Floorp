/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface DOMStringList;

interface HTMLPropertiesCollection : HTMLCollection {
  // inherits length and item()
  getter PropertyNodeList? namedItem(DOMString name); // overrides inherited namedItem()
  readonly attribute DOMStringList names;
};

typedef sequence<any> PropertyValueArray;

interface PropertyNodeList : NodeList {
  [Throws]
  PropertyValueArray getValues();
};
