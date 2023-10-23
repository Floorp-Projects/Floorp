/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-link-element
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

// https://html.spec.whatwg.org/multipage/semantics.html#the-link-element
[Exposed=Window]
interface HTMLLinkElement : HTMLElement {
  [HTMLConstructor] constructor();

  [CEReactions, SetterThrows, Pure]
           attribute boolean disabled;
  [CEReactions, SetterNeedsSubjectPrincipal=NonSystem, SetterThrows, Pure]
           attribute DOMString href;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString? crossOrigin;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString rel;
  [PutForwards=value]
  readonly attribute DOMTokenList relList;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString media;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString hreflang;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString type;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString referrerPolicy;
  [PutForwards=value] readonly attribute DOMTokenList sizes;
  [CEReactions, SetterThrows, Pure]
           attribute USVString imageSrcset;
  [CEReactions, SetterThrows, Pure]
           attribute USVString imageSizes;
  [Pref="network.fetchpriority.enabled", CEReactions]
           attribute DOMString fetchPriority;
};
HTMLLinkElement includes LinkStyle;

// https://html.spec.whatwg.org/multipage/obsolete.html#other-elements%2C-attributes-and-apis
partial interface HTMLLinkElement {
  [CEReactions, SetterThrows, Pure]
           attribute DOMString charset;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString rev;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString target;
};

// https://html.spec.whatwg.org/multipage/semantics.html#the-link-element
partial interface HTMLLinkElement {
  [CEReactions, SetterThrows]
  attribute DOMString integrity;
};

// https://html.spec.whatwg.org/multipage/links.html#link-type-preload
partial interface HTMLLinkElement {
  [SetterThrows, Pure]
           attribute DOMString as;
};
