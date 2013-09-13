/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[HeaderFile="mozilla/dom/InterAppComm.h",
 Func="mozilla::dom::InterAppComm::EnabledForScope",
 Constructor(DOMString keyword, MozInterAppMessagePort port),
 JSImplementation="@mozilla.org/dom/inter-app-connection-request;1"]
interface MozInterAppConnectionRequest {
  readonly attribute DOMString keyword;

  readonly attribute MozInterAppMessagePort port;
};
