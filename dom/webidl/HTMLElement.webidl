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

[Exposed=Window]
interface HTMLElement : Element {
  [HTMLConstructor] constructor();

  // metadata attributes
  [CEReactions]
           attribute DOMString title;
  [CEReactions]
           attribute DOMString lang;
  //         attribute boolean translate;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString dir;

  [CEReactions, GetterThrows, Pure]
           attribute [TreatNullAs=EmptyString] DOMString innerText;

  // user interaction
  [CEReactions, SetterThrows, Pure]
           attribute boolean hidden;
  [NeedsCallerType]
  void click();
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
  [Pure]
  readonly attribute HTMLMenuElement? contextMenu;
  //[SetterThrows]
  //         attribute HTMLMenuElement? contextMenu;
  [CEReactions, SetterThrows, Pure]
           attribute boolean spellcheck;
  [CEReactions, Pure, SetterThrows, Pref="dom.forms.inputmode"]
           attribute DOMString inputMode;

  attribute DOMString nonce;

  // command API
  //readonly attribute DOMString? commandType;
  //readonly attribute DOMString? commandLabel;
  //readonly attribute DOMString? commandIcon;
  //readonly attribute boolean? commandHidden;
  //readonly attribute boolean? commandDisabled;
  //readonly attribute boolean? commandChecked;

  // https://html.spec.whatwg.org/multipage/custom-elements.html#dom-attachinternals
  [Pref="dom.webcomponents.elementInternals.enabled", Throws]
  ElementInternals attachInternals();
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
HTMLElement includes DocumentAndElementEventHandlers;
HTMLElement includes ElementCSSInlineStyle;
HTMLElement includes TouchEventHandlers;
HTMLElement includes OnErrorEventHandlerForNodes;

[Exposed=Window]
interface HTMLUnknownElement : HTMLElement {};
