/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals exportFunction, module */

var UAHelpers = {
  getDeviceAppropriateChromeUA(chromeVersion = "76.0.3809.111") {
    if (!UAHelpers._deviceAppropriateChromeUA) {
      const userAgent =
        typeof navigator !== "undefined" ? navigator.userAgent : "";
      const RunningFirefoxVersion = (userAgent.match(/Firefox\/([0-9.]+)/) || [
        "",
        "58.0",
      ])[1];

      if (userAgent.includes("Android")) {
        const RunningAndroidVersion =
          userAgent.match(/Android\/[0-9.]+/) || "Android 6.0";
        const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${chromeVersion} Mobile Safari/537.36`;
        const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${chromeVersion} Safari/537.36`;
        const IsPhone = userAgent.includes("Mobile");
        UAHelpers._deviceAppropriateChromeUA = IsPhone
          ? ChromePhoneUA
          : ChromeTabletUA;
      } else {
        let osSegment = "Windows NT 10.0; Win64; x64";
        if (userAgent.includes("Macintosh")) {
          osSegment = "Macintosh; Intel Mac OS X 10_15_7";
        }
        if (userAgent.includes("Linux")) {
          osSegment = "X11; Ubuntu; Linux x86_64";
        }

        UAHelpers._deviceAppropriateChromeUA = `Mozilla/5.0 (${osSegment}) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${chromeVersion} Safari/537.36`;
      }
    }
    return UAHelpers._deviceAppropriateChromeUA;
  },
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
  overrideWithDeviceAppropriateChromeUA(chromeVersion = undefined) {
    const chromeUA = UAHelpers.getDeviceAppropriateChromeUA(chromeVersion);
    Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
      get: exportFunction(() => chromeUA, window),
      set: exportFunction(function() {}, window),
    });
  },
};

if (typeof module !== "undefined") {
  module.exports = UAHelpers;
}
