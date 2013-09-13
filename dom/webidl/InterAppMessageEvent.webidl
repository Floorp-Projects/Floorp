/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary MozInterAppMessageEventInit : EventInit {
  any data;
};

[HeaderFile="mozilla/dom/InterAppComm.h",
 Func="mozilla::dom::InterAppComm::EnabledForScope",
 Constructor(DOMString type,
             optional MozInterAppMessageEventInit eventInitDict),
 JSImplementation="@mozilla.org/dom/inter-app-message-event;1"]
interface MozInterAppMessageEvent : Event {
  readonly attribute any data;
};
