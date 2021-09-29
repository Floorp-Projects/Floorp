/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals exportFunction, module */

var UAHelpers = {
  getDeviceAppropriateChromeUA() {
    if (!UAHelpers._deviceAppropriateChromeUA) {
      const userAgent =
        typeof navigator !== "undefined" ? navigator.userAgent : "";
      const RunningFirefoxVersion = (userAgent.match(/Firefox\/([0-9.]+)/) || [
        "",
        "58.0",
      ])[1];
      const RunningAndroidVersion =
        userAgent.match(/Android\/[0-9.]+/) || "Android 6.0";
      const ChromeVersionToMimic = "76.0.3809.111";
      const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeVersionToMimic} Mobile Safari/537.36`;
      const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeVersionToMimic} Safari/537.36`;
      const IsPhone = userAgent.includes("Mobile");
      UAHelpers._deviceAppropriateChromeUA = IsPhone
        ? ChromePhoneUA
        : ChromeTabletUA;
    }
    return UAHelpers._deviceAppropriateChromeUA;
  },
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
  overrideWithDeviceAppropriateChromeUA() {
    const chromeUA = UAHelpers.getDeviceAppropriateChromeUA();
    Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
      get: exportFunction(() => chromeUA, window),
      set: exportFunction(function() {}, window),
    });
  },
};

if (typeof module !== "undefined") {
  module.exports = UAHelpers;
}
