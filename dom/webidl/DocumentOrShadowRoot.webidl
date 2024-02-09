/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dom.spec.whatwg.org/#documentorshadowroot
 * http://w3c.github.io/webcomponents/spec/shadow/#extensions-to-the-documentorshadowroot-mixin
 * https://wicg.github.io/construct-stylesheets/#using-constructed-stylesheets
 */

interface mixin DocumentOrShadowRoot {
  // Not implemented yet: bug 1430308.
  // Selection? getSelection();
  Element? elementFromPoint(float x, float y);
  sequence<Element> elementsFromPoint(float x, float y);

  // TODO: Avoid making these ChromeOnly, see:
  // https://github.com/w3c/csswg-drafts/issues/556
  [ChromeOnly]
  Node? nodeFromPoint(float x, float y);
  [ChromeOnly]
  sequence<Node> nodesFromPoint(float x, float y);

  // Not implemented yet: bug 1430307.
  // CaretPosition? caretPositionFromPoint (float x, float y);

  readonly attribute Element? activeElement;
  readonly attribute StyleSheetList styleSheets;

  readonly attribute Element? pointerLockElement;
  [LegacyLenientSetter]
  readonly attribute Element? fullscreenElement;
  [BinaryName="fullscreenElement"]
  readonly attribute Element? mozFullScreenElement;
};

// https://drafts.csswg.org/web-animations-1/#extensions-to-the-documentorshadowroot-interface-mixin
partial interface mixin DocumentOrShadowRoot {
  [Func="Document::IsWebAnimationsGetAnimationsEnabled"]
  sequence<Animation> getAnimations();
};

// https://wicg.github.io/construct-stylesheets/#using-constructed-stylesheets
partial interface mixin DocumentOrShadowRoot {
  // We are using [Pure, Cached, Frozen] sequence until `FrozenArray` is implemented.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1236777 for more details.
  attribute ObservableArray<CSSStyleSheet> adoptedStyleSheets;
};
