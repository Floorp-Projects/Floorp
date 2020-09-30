/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const flag = require("./test-flag");

function isBrowser() {
  return typeof window == "object";
}

function isNode() {
  return process && process.release && process.release.name == 'node'
}

function isDevelopment() {
  if (!isNode() && isBrowser()) {
    const href = window.location ? window.location.href : "";
    return href.match(/^file:/) || href.match(/localhost:/);
  }

  return process.env.NODE_ENV != "production";
}

function isTesting() {
  return flag.testing;
}

function isFirefoxPanel() {
  return !isDevelopment();
}

function isFirefox() {
  return /firefox/i.test(navigator.userAgent);
}


module.exports = {
  isDevelopment,
  isTesting,
  isFirefoxPanel,
  isFirefox
}
