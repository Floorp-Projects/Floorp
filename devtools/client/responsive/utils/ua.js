/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const BROWSERS = [
  {
    name: "Firefox",
    mustContain: new RegExp(`(?:Firefox|FxiOS)\/(${getVersionRegex(1, 1)})`),
  },
  {
    name: "Opera",
    mustContain: new RegExp(`(?:OPR|Opera)\/(${getVersionRegex(1, 1)})`),
  },
  {
    name: "Safari",
    mustContain: new RegExp(`Version\/(${getVersionRegex(1, 1)}).+Safari`),
    mustNotContain: new RegExp("Chrome|Chromium"),
  },
  {
    name: "Edge",
    mustContain: new RegExp(`Edge\/(${getVersionRegex(0, 1)})`),
  },
  {
    name: "Chrome",
    mustContain: new RegExp(`(?:Chrome|CriOS)\/(${getVersionRegex(1, 1)})`),
  },
  {
    name: "IE",
    mustContain: new RegExp(`MSIE (${getVersionRegex(1, 1)})`),
  },
];

const OSES = [
  {
    name: "iOS",
    minMinorVersionCount: 0,
    mustContain: new RegExp(`CPU iPhone OS (${getVersionRegex(0, 2)})`),
  },
  {
    name: "iPadOS",
    minMinorVersionCount: 0,
    mustContain: new RegExp(`CPU OS (${getVersionRegex(0, 2)})`),
  },
  {
    name: "Windows Phone",
    minMinorVersionCount: 1,
    mustContain: new RegExp(`Windows Phone (${getVersionRegex(1, 2)})`),
  },
  {
    name: "Chrome OS",
    minMinorVersionCount: 1,
    mustContain: new RegExp(`CrOS .+ (${getVersionRegex(1, 2)})`),
  },
  {
    name: "Android",
    minMinorVersionCount: 0,
    mustContain: new RegExp(`Android (${getVersionRegex(0, 2)})`),
  },
  {
    name: "Windows NT",
    minMinorVersionCount: 1,
    mustContain: new RegExp(`Windows NT (${getVersionRegex(1, 2)})`),
  },
  {
    name: "Mac OSX",
    minMinorVersionCount: 1,
    mustContain: new RegExp(`Intel Mac OS X (${getVersionRegex(1, 2)})`),
  },
  {
    name: "Linux",
    mustContain: new RegExp("Linux"),
  },
];

function getVersionRegex(minMinorVersionCount, maxMinorVersionCount) {
  return `\\d+(?:[._][0-9a-z]+){${minMinorVersionCount},${maxMinorVersionCount}}`;
}

function detect(ua, dataset) {
  for (const {
    name,
    mustContain,
    mustNotContain,
    minMinorVersionCount,
  } of dataset) {
    const result = mustContain.exec(ua);

    if (!result) {
      continue;
    }

    if (mustNotContain && mustNotContain.test(ua)) {
      continue;
    }

    let version = null;

    if (result && result.length === 2) {
      // Remove most minor version if that expresses 0.
      let parts = result[1].match(/([0-9a-z]+)/g);
      parts = parts.reverse();
      const validVersionIndex = parts.findIndex(
        part => parseInt(part, 10) !== 0
      );
      if (validVersionIndex !== -1) {
        parts = parts.splice(validVersionIndex);
        for (let i = 0; i < minMinorVersionCount + 1 - parts.length; i++) {
          parts.unshift(0);
        }
      }
      version = parts.reverse().join(".");
    }

    return { name, version };
  }

  return null;
}

function parseUserAgent(ua) {
  return {
    browser: detect(ua, BROWSERS),
    os: detect(ua, OSES),
  };
}

module.exports = { parseUserAgent };
