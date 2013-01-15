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

  // event handler IDL attributes
  [SetterThrows]
           attribute EventHandler onabort;
  [SetterThrows]
           attribute EventHandler onblur;
  //[SetterThrows]
  //         attribute EventHandler oncancel;
  [SetterThrows]
           attribute EventHandler oncanplay;
  [SetterThrows]
           attribute EventHandler oncanplaythrough;
  [SetterThrows]
           attribute EventHandler onchange;
  [SetterThrows]
           attribute EventHandler onclick;
  //[SetterThrows]
  //         attribute EventHandler onclose;
  [SetterThrows]
           attribute EventHandler oncontextmenu;
  //[SetterThrows]
  //         attribute EventHandler oncuechange;
  [SetterThrows]
           attribute EventHandler ondblclick;
  [SetterThrows]
           attribute EventHandler ondrag;
  [SetterThrows]
           attribute EventHandler ondragend;
  [SetterThrows]
           attribute EventHandler ondragenter;
  [SetterThrows]
           attribute EventHandler ondragleave;
  [SetterThrows]
           attribute EventHandler ondragover;
  [SetterThrows]
           attribute EventHandler ondragstart;
  [SetterThrows]
           attribute EventHandler ondrop;
  [SetterThrows]
           attribute EventHandler ondurationchange;
  [SetterThrows]
           attribute EventHandler onemptied;
  [SetterThrows]
           attribute EventHandler onended;
  // We think the spec is wrong here.
  //         attribute OnErrorEventHandler onerror;
  [SetterThrows]
           attribute EventHandler onerror;
  [SetterThrows]
           attribute EventHandler onfocus;
  [SetterThrows]
           attribute EventHandler oninput;
  [SetterThrows]
           attribute EventHandler oninvalid;
  [SetterThrows]
           attribute EventHandler onkeydown;
  [SetterThrows]
           attribute EventHandler onkeypress;
  [SetterThrows]
           attribute EventHandler onkeyup;
  [SetterThrows]
           attribute EventHandler onload;
  [SetterThrows]
           attribute EventHandler onloadeddata;
  [SetterThrows]
           attribute EventHandler onloadedmetadata;
  [SetterThrows]
           attribute EventHandler onloadstart;
  [SetterThrows]
           attribute EventHandler onmousedown;
  [SetterThrows]
           attribute EventHandler onmousemove;
  [SetterThrows]
           attribute EventHandler onmouseout;
  [SetterThrows]
           attribute EventHandler onmouseover;
  [SetterThrows]
           attribute EventHandler onmouseup;
  //[SetterThrows]
  //         attribute EventHandler onmousewheel;
  [SetterThrows]
           attribute EventHandler onpause;
  [SetterThrows]
           attribute EventHandler onplay;
  [SetterThrows]
           attribute EventHandler onplaying;
  [SetterThrows]
           attribute EventHandler onprogress;
  [SetterThrows]
           attribute EventHandler onratechange;
  [SetterThrows]
           attribute EventHandler onreset;
  [SetterThrows]
           attribute EventHandler onscroll;
  [SetterThrows]
           attribute EventHandler onseeked;
  [SetterThrows]
           attribute EventHandler onseeking;
  [SetterThrows]
           attribute EventHandler onselect;
  [SetterThrows]
           attribute EventHandler onshow;
  [SetterThrows]
           attribute EventHandler onstalled;
  [SetterThrows]
           attribute EventHandler onsubmit;
  [SetterThrows]
           attribute EventHandler onsuspend;
  [SetterThrows]
           attribute EventHandler ontimeupdate;
  [SetterThrows]
           attribute EventHandler onvolumechange;
  [SetterThrows]
           attribute EventHandler onwaiting;

  [SetterThrows]
           attribute EventHandler onmozfullscreenchange;
  [SetterThrows]
           attribute EventHandler onmozfullscreenerror;
  [SetterThrows]
           attribute EventHandler onmozpointerlockchange;
  [SetterThrows]
           attribute EventHandler onmozpointerlockerror;

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
