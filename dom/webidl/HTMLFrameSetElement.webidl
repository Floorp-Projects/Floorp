/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

interface HTMLFrameSetElement : HTMLElement {
  [SetterThrows]
  attribute DOMString cols;
  [SetterThrows]
  attribute DOMString rows;
  [SetterThrows]
  attribute EventHandler onafterprint;
  [SetterThrows]
  attribute EventHandler onbeforeprint;
  [SetterThrows]
  attribute BeforeUnloadEventHandler onbeforeunload;
  //[SetterThrows]
  //attribute EventHandler onblur;
  //[SetterThrows]
  //attribute EventHandler onerror;
  //[SetterThrows]
  //attribute EventHandler onfocus;
  [SetterThrows]
  attribute EventHandler onhashchange;
  //attribute EventHandler onload;
  [SetterThrows]
  attribute EventHandler onmessage;
  [SetterThrows]
  attribute EventHandler onoffline;
  [SetterThrows]
  attribute EventHandler ononline;
  [SetterThrows]
  attribute EventHandler onpagehide;
  [SetterThrows]
  attribute EventHandler onpageshow;
  [SetterThrows]
  attribute EventHandler onpopstate;
  [SetterThrows]
  attribute EventHandler onresize;
  //[SetterThrows]
  //attribute EventHandler onscroll;
  //[SetterThrows]
  //attribute EventHandler onstorage;
  [SetterThrows]
  attribute EventHandler onunload;
};
