/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/obsolete.html#the-marquee-element
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

// https://html.spec.whatwg.org/multipage/obsolete.html#the-marquee-element

[Exposed=Window]
interface HTMLMarqueeElement : HTMLElement {
  [HTMLConstructor] constructor();

  [CEReactions, SetterThrows] attribute DOMString behavior;
  [CEReactions, SetterThrows] attribute DOMString bgColor;
  [CEReactions, SetterThrows] attribute DOMString direction;
  [CEReactions, SetterThrows] attribute DOMString height;
  [CEReactions, SetterThrows] attribute unsigned long hspace;
  [CEReactions, SetterThrows] attribute long loop;
  [CEReactions, SetterThrows] attribute unsigned long scrollAmount;
  [CEReactions, SetterThrows] attribute unsigned long scrollDelay;
  [CEReactions, SetterThrows] attribute boolean trueSpeed;
  [CEReactions, SetterThrows] attribute unsigned long vspace;
  [CEReactions, SetterThrows] attribute DOMString width;

  attribute EventHandler onbounce;
  attribute EventHandler onfinish;
  attribute EventHandler onstart;

  void start();
  void stop();
};

