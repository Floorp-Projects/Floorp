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
  [Constant]
  readonly attribute DOMStringMap dataset;

  // microdata 
  [SetterThrows]
           attribute boolean itemScope;
  [PutForwards=value,Constant] readonly attribute DOMSettableTokenList itemType;
  [SetterThrows]
           attribute DOMString itemId;
  [PutForwards=value,Constant] readonly attribute DOMSettableTokenList itemRef;
  [PutForwards=value,Constant] readonly attribute DOMSettableTokenList itemProp;
  [Constant]
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
  [Constant]
  readonly attribute CSSStyleDeclaration style;

  // event handler IDL attributes
  [SetterThrows]
           attribute EventHandler onblur;
  // We think the spec is wrong here.
  //         attribute OnErrorEventHandler onerror;
  [SetterThrows]
           attribute EventHandler onerror;
  [SetterThrows]
           attribute EventHandler onfocus;
  [SetterThrows]
           attribute EventHandler onload;
  [SetterThrows]
           attribute EventHandler onscroll;

  // Mozilla specific stuff
  // FIXME Bug 810677 Move className from HTMLElement to Element
           attribute DOMString className;

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

// http://dev.w3.org/csswg/cssom-view/#extensions-to-the-htmlelement-interface
partial interface HTMLElement {
  readonly attribute Element? offsetParent;
  readonly attribute long offsetTop;
  readonly attribute long offsetLeft;
  readonly attribute long offsetWidth;
  readonly attribute long offsetHeight;
};

HTMLElement implements GlobalEventHandlers;

interface HTMLUnknownElement : HTMLElement {};
