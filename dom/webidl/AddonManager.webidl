/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* We need a JSImplementation but cannot get one without a contract ID. Since
   This object is only ever created from JS we don't need a real contract ID. */
[ChromeOnly, JSImplementation="dummy"]
interface Addon {
  // The add-on's ID.
  readonly attribute DOMString id;
  // The add-on's version.
  readonly attribute DOMString version;
  // The add-on's type (extension, theme, etc.).
  readonly attribute DOMString type;
  // The add-on's name in the current locale.
  readonly attribute DOMString name;
  // The add-on's description in the current locale.
  readonly attribute DOMString description;
  // If the user has enabled this add-on, note that it still may not be running
  // depending on whether enabling requires a restart or if the add-on is
  // incompatible in some way.
  readonly attribute boolean isEnabled;
  // If the add-on is currently active in the browser.
  readonly attribute boolean isActive;
};

[HeaderFile="mozilla/AddonManagerWebAPI.h",
 Func="mozilla::AddonManagerWebAPI::IsAPIEnabled",
 NavigatorProperty="mozAddonManager",
 JSImplementation="@mozilla.org/addon-web-api/manager;1"]
interface AddonManager {
  /**
   * Gets information about an add-on
   *
   * @param  id
   *         The ID of the add-on to test for.
   * @return A promise. It will resolve to an Addon if the add-on is installed.
   */
  Promise<Addon> getAddonByID(DOMString id);
};
