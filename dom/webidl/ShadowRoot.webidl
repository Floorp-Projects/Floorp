/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/webcomponents/raw-file/tip/spec/shadow/index.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

// https://dom.spec.whatwg.org/#enumdef-shadowrootmode
enum ShadowRootMode {
  "open",
  "closed"
};

// https://dom.spec.whatwg.org/#shadowroot
[Exposed=Window]
interface ShadowRoot : DocumentFragment
{
  // Shadow DOM v1
  readonly attribute ShadowRootMode mode;
  readonly attribute Element host;

  Element? getElementById(DOMString elementId);

  // https://w3c.github.io/DOM-Parsing/#the-innerhtml-mixin
  [CEReactions, SetterThrows]
  attribute [LegacyNullToEmptyString] DOMString innerHTML;

  // When JS invokes importNode or createElement, the binding code needs to
  // create a reflector, and so invoking those methods directly on the content
  // document would cause the reflector to be created in the content scope,
  // at which point it would be difficult to move into the UA Widget scope.
  // As such, these methods allow UA widget code to simultaneously create nodes
  // and associate them with the UA widget tree, so that the reflectors get
  // created in the right scope.
  [CEReactions, Throws, Func="IsChromeOrUAWidget"]
  Node importNodeAndAppendChildAt(Node parentNode, Node node, optional boolean deep = false);

  [CEReactions, Throws, Func="IsChromeOrUAWidget"]
  Node createElementAndAppendChildAt(Node parentNode, DOMString localName);

  // For triggering UA Widget scope in tests.
  [ChromeOnly]
  void setIsUAWidget();
  [ChromeOnly]
  boolean isUAWidget();
};

ShadowRoot includes DocumentOrShadowRoot;
