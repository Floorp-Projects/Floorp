/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://immersive-web.github.io/webxr/#xrinputsourceschangeevent-interface
 */

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRInputSourcesChangeEvent : Event {
  constructor(DOMString type, XRInputSourcesChangeEventInit eventInitDict);
  [SameObject] readonly attribute XRSession session;
  // TODO: Use FrozenArray once available. (Bug 1236777)
  [Constant, Cached, Frozen]
  readonly attribute sequence<XRInputSource> added;
  // TODO: Use FrozenArray once available. (Bug 1236777)
  [Constant, Cached, Frozen]
  readonly attribute sequence<XRInputSource> removed;
};

dictionary XRInputSourcesChangeEventInit : EventInit {
  required XRSession session;
  // TODO: Use FrozenArray once available. (Bug 1236777)
  required sequence<XRInputSource> added;
  // TODO: Use FrozenArray once available. (Bug 1236777)
  required sequence<XRInputSource> removed;
};
