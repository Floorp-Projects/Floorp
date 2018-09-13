/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum VRDisplayEventReason {
  "mounted",
  "navigation",
  "requested",
  "unmounted",
};

dictionary VRDisplayEventInit : EventInit {
  required VRDisplay display;
  VRDisplayEventReason reason;
};

[Pref="dom.vr.enabled",
 Constructor(DOMString type, VRDisplayEventInit eventInitDict)]
interface VRDisplayEvent : Event {
  readonly attribute VRDisplay display;
  readonly attribute VRDisplayEventReason? reason;
};
