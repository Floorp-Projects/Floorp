/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG2/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface SVGAnimatedString;

[PrefControlled]
interface SVGElement : Element {
           attribute DOMString id;
/*           [SetterThrows]
           attribute DOMString xmlbase; */

  readonly attribute SVGAnimatedString className;
  readonly attribute CSSStyleDeclaration style;

  // The CSSValue interface has been deprecated by the CSS WG.
  // http://lists.w3.org/Archives/Public/www-style/2003Oct/0347.html
  // CSSValue? getPresentationAttribute(DOMString name);

  /*[SetterThrows]
  attribute DOMString xmllang;
  [SetterThrows]
  attribute DOMString xmlspace;*/

  [Throws]
  readonly attribute SVGSVGElement? ownerSVGElement;
  readonly attribute SVGElement? viewportElement;

  [SetterThrows,Pref="dom.w3c_touch_events.expose"]
           attribute EventHandler ontouchstart;
  [SetterThrows,Pref="dom.w3c_touch_events.expose"]
           attribute EventHandler ontouchend;
  [SetterThrows,Pref="dom.w3c_touch_events.expose"]
           attribute EventHandler ontouchmove;
  [SetterThrows,Pref="dom.w3c_touch_events.expose"]
           attribute EventHandler ontouchenter;
  [SetterThrows,Pref="dom.w3c_touch_events.expose"]
           attribute EventHandler ontouchleave;
  [SetterThrows,Pref="dom.w3c_touch_events.expose"]
           attribute EventHandler ontouchcancel;

  [SetterThrows]
           attribute EventHandler oncopy;
  [SetterThrows]
           attribute EventHandler oncut;
  [SetterThrows]
           attribute EventHandler onpaste;
};

SVGElement implements GlobalEventHandlers;
SVGElement implements NodeEventHandlers;
