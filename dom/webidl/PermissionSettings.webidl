/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain at http://mozilla.org/MPL/2.0/. */

[JSImplementation="@mozilla.org/permissionSettings;1",
 CheckPermissions="permissions",
 Pref="dom.mozPermissionSettings.enabled",
 NavigatorProperty="mozPermissionSettings"]
interface PermissionSettings
{
  DOMString get(DOMString permission, DOMString manifestURI, DOMString origin, boolean browserFlag);

  void set(DOMString permission, DOMString value, DOMString manifestURI, DOMString origin, boolean browserFlag);

  boolean isExplicit(DOMString permission, DOMString manifestURI, DOMString origin, boolean browserFlag);

  // Removing a permission is only allowed for pages with a different origin than the app
  // and pages that have browserFlag=true, so remove() doesn't have a browserFlag parameter.
  void remove(DOMString permission, DOMString manifestURI, DOMString origin);
};
