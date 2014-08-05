/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * www.w3.org/TR/2012/WD-XMLHttpRequest-20120117/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[NoInterfaceObject,
 Exposed=(Window,Worker)]
interface XMLHttpRequestEventTarget : EventTarget {
  // event handlers
  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onloadstart;

  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onprogress;

  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onabort;

  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onerror;

  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onload;

  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler ontimeout;

  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onloadend;
};
