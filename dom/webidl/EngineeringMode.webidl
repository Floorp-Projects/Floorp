/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[JSImplementation="@mozilla.org/dom/engineering-mode-api;1",
 NavigatorProperty="engineeringMode",
 AvailableIn=CertifiedApps,
 CheckAnyPermissions="engineering-mode"]
interface EngineeringMode : EventTarget {
  Promise<DOMString> getValue(DOMString name);
  Promise<void> setValue(DOMString name, DOMString value);
  attribute EventHandler onmessage;
};
