/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

[Func="mozilla::AddonManagerWebAPI::IsAPIEnabled",
 Exposed=Window]
interface AddonEvent : Event {
  constructor(DOMString type, AddonEventInit eventInitDict);

  readonly attribute DOMString id;
};

dictionary AddonEventInit : EventInit {
  required DOMString id;
};
