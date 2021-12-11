/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-dom-20120105/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=(Window,Worker), ProbablyShortLivingWrapper]
interface Event {
  constructor(DOMString type, optional EventInit eventInitDict = {});

  [Pure]
  readonly attribute DOMString type;
  [Pure, BindingAlias="srcElement"]
  readonly attribute EventTarget? target;
  [Pure]
  readonly attribute EventTarget? currentTarget;

  sequence<EventTarget> composedPath();

  const unsigned short NONE = 0;
  const unsigned short CAPTURING_PHASE = 1;
  const unsigned short AT_TARGET = 2;
  const unsigned short BUBBLING_PHASE = 3;
  [Pure]
  readonly attribute unsigned short eventPhase;

  void stopPropagation();
  void stopImmediatePropagation();

  [Pure]
  readonly attribute boolean bubbles;
  [Pure]
  readonly attribute boolean cancelable;
  [NeedsCallerType]
  attribute boolean returnValue;
  [NeedsCallerType]
  void preventDefault();
  [Pure, NeedsCallerType]
  readonly attribute boolean defaultPrevented;
  [ChromeOnly, Pure]
  readonly attribute boolean defaultPreventedByChrome;
  [ChromeOnly, Pure]
  readonly attribute boolean defaultPreventedByContent;
  [Pure]
  readonly attribute boolean composed;

  [LegacyUnforgeable, Pure]
  readonly attribute boolean isTrusted;
  [Pure]
  readonly attribute DOMHighResTimeStamp timeStamp;

  void initEvent(DOMString type,
                 optional boolean bubbles = false,
                 optional boolean cancelable = false);
  attribute boolean cancelBubble;
};

// Mozilla specific legacy stuff.
partial interface Event {
  const long ALT_MASK     = 0x00000001;
  const long CONTROL_MASK = 0x00000002;
  const long SHIFT_MASK   = 0x00000004;
  const long META_MASK    = 0x00000008;

  /** The original target of the event, before any retargetings. */
  readonly attribute EventTarget? originalTarget;
  /**
   * The explicit original target of the event.  If the event was retargeted
   * for some reason other than an anonymous boundary crossing, this will be set
   * to the target before the retargeting occurs.  For example, mouse events
   * are retargeted to their parent node when they happen over text nodes (bug
   * 185889), and in that case .target will show the parent and
   * .explicitOriginalTarget will show the text node.
   * .explicitOriginalTarget differs from .originalTarget in that it will never
   * contain anonymous content.
   */
  readonly attribute EventTarget? explicitOriginalTarget;
  [ChromeOnly] readonly attribute EventTarget? composedTarget;
  [ChromeOnly] void preventMultipleActions();
  [ChromeOnly] readonly attribute boolean multipleActionsPrevented;
  [ChromeOnly] readonly attribute boolean isSynthesized;
  /**
   * When the event target is a remote browser, calling this will fire an
   * reply event in the chrome process.
   */
  [ChromeOnly] void requestReplyFromRemoteContent();
  /**
   * Returns true when the event shouldn't be handled by chrome.
   */
  [ChromeOnly] readonly attribute boolean isWaitingReplyFromRemoteContent;
  /**
   * Returns true when the event is a reply event from a remote process.
   */
  [ChromeOnly] readonly attribute boolean isReplyEventFromRemoteContent;
};

dictionary EventInit {
  boolean bubbles = false;
  boolean cancelable = false;
  boolean composed = false;
};
