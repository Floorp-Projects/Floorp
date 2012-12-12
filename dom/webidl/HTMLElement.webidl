/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/ and
 * http://dev.w3.org/csswg/cssom-view/
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

interface DOMStringMap;
interface HTMLMenuElement;

[PrefControlled]
interface HTMLElement : Element {
  // metadata attributes
           attribute DOMString title;
           attribute DOMString lang;
  //         attribute boolean translate;
  [SetterThrows]
           attribute DOMString dir;
  readonly attribute DOMStringMap dataset;

  // microdata 
  [SetterThrows]
           attribute boolean itemScope;
  [PutForwards=value] readonly attribute DOMSettableTokenList itemType;
  [SetterThrows]
           attribute DOMString itemId;
  [PutForwards=value] readonly attribute DOMSettableTokenList itemRef;
  [PutForwards=value] readonly attribute DOMSettableTokenList itemProp;
  readonly attribute HTMLPropertiesCollection properties;
  [Throws]
           attribute any itemValue;

  // user interaction
  [SetterThrows]
           attribute boolean hidden;
  void click();
  [SetterThrows]
           attribute long tabIndex;
  [Throws]
  void focus();
  [Throws]
  void blur();
  [SetterThrows]
           attribute DOMString accessKey;
  readonly attribute DOMString accessKeyLabel;
  [SetterThrows]
           attribute boolean draggable;
  //[PutForwards=value] readonly attribute DOMSettableTokenList dropzone;
  [SetterThrows]
           attribute DOMString contentEditable;
  readonly attribute boolean isContentEditable;
  readonly attribute HTMLMenuElement? contextMenu;
  //[SetterThrows]
  //         attribute HTMLMenuElement? contextMenu;
  [SetterThrows]
           attribute boolean spellcheck;

  // command API
  //readonly attribute DOMString? commandType;
  //readonly attribute DOMString? commandLabel;
  //readonly attribute DOMString? commandIcon;
  //readonly attribute boolean? commandHidden;
  //readonly attribute boolean? commandDisabled;
  //readonly attribute boolean? commandChecked;

  // styling
  [Throws]
  readonly attribute CSSStyleDeclaration style;

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

  // Mozilla specific stuff
  // FIXME Bug 810677 Move className from HTMLElement to Element
           attribute DOMString className;

  /* Commented out for now because our quickstub setup doesn't handle calling PreEnabled() on our interface, which is what sets up the .expose pref here
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
           attribute EventHandler ontouchcancel;*/

  [SetterThrows]
           attribute EventHandler oncopy;
  [SetterThrows]
           attribute EventHandler oncut;
  [SetterThrows]
           attribute EventHandler onpaste;

  // FIXME Bug 811701 Move innerHTML/outerHTML/insertAdjacentHTML from
  //                  HTMLElement to Element
  [Throws,TreatNullAs=EmptyString]
  attribute DOMString innerHTML;
  [Throws,TreatNullAs=EmptyString]
  attribute DOMString outerHTML;
  [Throws]
  void insertAdjacentHTML(DOMString position, DOMString text);
/*
};

// http://dev.w3.org/csswg/cssom-view/#extensions-to-the-htmlelement-interface
partial interface HTMLElement {
*/
  readonly attribute Element? offsetParent;
  readonly attribute long offsetTop;
  readonly attribute long offsetLeft;
  readonly attribute long offsetWidth;
  readonly attribute long offsetHeight;
};

interface HTMLUnknownElement : HTMLElement {};
