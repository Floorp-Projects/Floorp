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

[Exposed=Window,
 InstrumentedProps=(attributeStyleMap,hidePopover,popover,showPopover,togglePopover)]
interface HTMLElement : Element {
  [HTMLConstructor] constructor();

  // metadata attributes
  [CEReactions]
           attribute DOMString title;
  [CEReactions]
           attribute DOMString lang;
  [CEReactions, SetterThrows, Pure]
           attribute boolean translate;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString dir;

  [CEReactions, GetterThrows, Pure]
           attribute [LegacyNullToEmptyString] DOMString innerText;
  [CEReactions, GetterThrows, SetterThrows, Pure]
           attribute [LegacyNullToEmptyString] DOMString outerText;

  // user interaction
  [CEReactions, SetterThrows, Pure]
           attribute boolean hidden;
  [CEReactions, SetterThrows, Pure]
           attribute boolean inert;
  [NeedsCallerType]
  undefined click();
  [CEReactions, SetterThrows, Pure]
           attribute DOMString accessKey;
  [Pure]
  readonly attribute DOMString accessKeyLabel;
  [CEReactions, SetterThrows, Pure]
           attribute boolean draggable;
  //[PutForwards=value] readonly attribute DOMTokenList dropzone;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString contentEditable;
  [Pure]
  readonly attribute boolean isContentEditable;
  [CEReactions, SetterThrows, Pure, Pref="dom.element.popover.enabled"]
           attribute DOMString? popover;
  [CEReactions, SetterThrows, Pure]
           attribute boolean spellcheck;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString inputMode;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString enterKeyHint;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString autocapitalize;

  attribute DOMString nonce;

  // command API
  //readonly attribute DOMString? commandType;
  //readonly attribute DOMString? commandLabel;
  //readonly attribute DOMString? commandIcon;
  //readonly attribute boolean? commandHidden;
  //readonly attribute boolean? commandDisabled;
  //readonly attribute boolean? commandChecked;

  // https://html.spec.whatwg.org/multipage/custom-elements.html#dom-attachinternals
  [Throws]
  ElementInternals attachInternals();

  [Throws, Pref="dom.element.popover.enabled"]
  undefined showPopover();
  [Throws, Pref="dom.element.popover.enabled"]
  undefined hidePopover();
  [Throws, Pref="dom.element.popover.enabled"]
  boolean togglePopover(optional boolean force);
};

// http://dev.w3.org/csswg/cssom-view/#extensions-to-the-htmlelement-interface
partial interface HTMLElement {
  // CSSOM things are not [Pure] because they can flush
  readonly attribute Element? offsetParent;
  readonly attribute long offsetTop;
  readonly attribute long offsetLeft;
  readonly attribute long offsetWidth;
  readonly attribute long offsetHeight;
};

partial interface HTMLElement {
  [ChromeOnly]
  readonly attribute ElementInternals? internals;

  [ChromeOnly]
  readonly attribute boolean isFormAssociatedCustomElements;
};

interface mixin TouchEventHandlers {
  [Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
           attribute EventHandler ontouchstart;
  [Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
           attribute EventHandler ontouchend;
  [Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
           attribute EventHandler ontouchmove;
  [Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
           attribute EventHandler ontouchcancel;
};

HTMLElement includes GlobalEventHandlers;
HTMLElement includes HTMLOrForeignElement;
HTMLElement includes ElementCSSInlineStyle;
HTMLElement includes TouchEventHandlers;
HTMLElement includes OnErrorEventHandlerForNodes;

[Exposed=Window]
interface HTMLUnknownElement : HTMLElement {};
