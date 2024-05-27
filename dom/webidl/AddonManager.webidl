/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* We need a JSImplementation but cannot get one without a contract ID.
   Since Addon and AddonInstall are only ever created from JS they don't need
   real contract IDs. */
[ChromeOnly, JSImplementation="dummy",
 Exposed=Window]
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
  // If the add-on may be uninstalled
  readonly attribute boolean canUninstall;

  Promise<boolean> uninstall();
  Promise<undefined> setEnabled(boolean value);
};

[ChromeOnly, JSImplementation="dummy",
 Exposed=Window]
interface AddonInstall : EventTarget {
  // One of the STATE_* symbols from AddonManager.sys.mjs
  readonly attribute DOMString state;
  // One of the ERROR_* symbols from AddonManager.sys.mjs, or null
  readonly attribute DOMString? error;
  // How many bytes have been downloaded
  readonly attribute long long progress;
  // How many total bytes will need to be downloaded or -1 if unknown
  readonly attribute long long maxProgress;

  Promise<undefined> install();
  Promise<undefined> cancel();
};

dictionary addonInstallOptions {
  required DOMString url;
  // If a non-empty string is passed for "hash", it is used to verify the
  // checksum of the downloaded XPI before installing.  If is omitted or if
  // it is null or empty string, no checksum verification is performed.
  DOMString? hash = null;
};

dictionary sendAbuseReportOptions {
  // This should be an Authorization HTTP header value.
  DOMString? authorization = null;
};

[HeaderFile="mozilla/AddonManagerWebAPI.h",
 Func="mozilla::AddonManagerWebAPI::IsAPIEnabled",
 JSImplementation="@mozilla.org/addon-web-api/manager;1",
 WantsEventListenerHooks,
 Exposed=Window]
interface AddonManager : EventTarget {
  /**
   * Gets information about an add-on
   *
   * @param  id
   *         The ID of the add-on to test for.
   * @return A promise. It will resolve to an Addon if the add-on is installed.
   */
  Promise<Addon> getAddonByID(DOMString id);

  /**
   * Creates an AddonInstall object for a given URL.
   *
   * @param options
   *        Only one supported option: 'url', the URL of the addon to install.
   * @return A promise that resolves to an instance of AddonInstall.
   */
  Promise<AddonInstall> createInstall(optional addonInstallOptions options = {});

  /**
   * Sends an abuse report to the AMO API.
   *
   * NOTE: The type for `data` and for the return value are loose because both
   * the AMO API might change its response and the caller (AMO frontend) might
   * also want to pass slightly different data in the future.
   *
   * @param addonId
   *        The ID of the add-on to report.
   * @param data
   *        The caller passes the data to be sent to the AMO API.
   * @param options
   *        Optional - A set of options. It currently only supports
   *        `authorization`, which is expected to be the value of an
   *        Authorization HTTP header when provided.
   * @return A promise that resolves to the AMO API response, or an error when
   *         something went wrong.
   */
  [NewObject] Promise<any> sendAbuseReport(
    DOMString addonId,
    record<DOMString, DOMString?> data,
    optional sendAbuseReportOptions options = {}
  );
};

[ChromeOnly,Exposed=Window,HeaderFile="mozilla/AddonManagerWebAPI.h"]
namespace AddonManagerPermissions {
  boolean isHostPermitted(DOMString host);
};
