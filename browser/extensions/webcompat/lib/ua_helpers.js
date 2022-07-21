/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals exportFunction, module */

var UAHelpers = {
  _deviceAppropriateChromeUAs: {},
  getDeviceAppropriateChromeUA(config = {}) {
    const { version = "103.0.5060.71", androidDevice, desktopOS } = config;
    const key = `${version}:${androidDevice}:${desktopOS}`;
    if (!UAHelpers._deviceAppropriateChromeUAs[key]) {
      const userAgent =
        typeof navigator !== "undefined" ? navigator.userAgent : "";
      const RunningFirefoxVersion = (userAgent.match(/Firefox\/([0-9.]+)/) || [
        "",
        "58.0",
      ])[1];

      if (userAgent.includes("Android")) {
        const RunningAndroidVersion =
          userAgent.match(/Android\/[0-9.]+/) || "Android 6.0";
        if (androidDevice) {
          UAHelpers._deviceAppropriateChromeUAs[
            key
          ] = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; ${androidDevice}) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Mobile Safari/537.36`;
        } else {
          const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Mobile Safari/537.36`;
          const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Safari/537.36`;
          const IsPhone = userAgent.includes("Mobile");
          UAHelpers._deviceAppropriateChromeUAs[key] = IsPhone
            ? ChromePhoneUA
            : ChromeTabletUA;
        }
      } else {
        let osSegment = "Windows NT 10.0; Win64; x64";
        if (desktopOS === "macOS" || userAgent.includes("Macintosh")) {
          osSegment = "Macintosh; Intel Mac OS X 10_15_7";
        }
        if (
          desktopOS !== "nonLinux" &&
          (desktopOS === "linux" || userAgent.includes("Linux"))
        ) {
          osSegment = "X11; Ubuntu; Linux x86_64";
        }

        UAHelpers._deviceAppropriateChromeUAs[
          key
        ] = `Mozilla/5.0 (${osSegment}) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Safari/537.36`;
      }
    }
    return UAHelpers._deviceAppropriateChromeUAs[key];
  },
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
  overrideWithDeviceAppropriateChromeUA(config) {
    const chromeUA = UAHelpers.getDeviceAppropriateChromeUA(config);
    Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
      get: exportFunction(() => chromeUA, window),
      set: exportFunction(function() {}, window),
    });
  },
  capVersionTo99(originalUA) {
    const ver = originalUA.match(/Firefox\/(\d+\.\d+)/);
    if (!ver || parseFloat(ver[1]) < 100) {
      return originalUA;
    }
    return originalUA
      .replace(`Firefox/${ver[1]}`, "Firefox/99.0")
      .replace(`rv:${ver[1]}`, "rv:99.0");
  },
};

if (typeof module !== "undefined") {
  module.exports = UAHelpers;
}
