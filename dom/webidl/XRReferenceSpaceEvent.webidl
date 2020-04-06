/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://immersive-web.github.io/webxr/#xrreferencespaceevent-interface
 */

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRReferenceSpaceEvent : Event {
  constructor(DOMString type, XRReferenceSpaceEventInit eventInitDict);
  [SameObject] readonly attribute XRReferenceSpace referenceSpace;
  [SameObject] readonly attribute XRRigidTransform? transform;
};

dictionary XRReferenceSpaceEventInit : EventInit {
  required XRReferenceSpace referenceSpace;
  /*
   * Changed from "XRRigidTransform transform;" in order to work with the
   * event code generation.
   */
  XRRigidTransform? transform = null;
};
