/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "browsersDataset",
  "devtools/client/inspector/compatibility/lib/dataset/browsers.json"
);

const TARGET_BROWSER_ID = [
  "firefox",
  "firefox_android",
  "chrome",
  "chrome_android",
  "safari",
  "safari_ios",
  "edge",
];
const TARGET_BROWSER_STATUS = ["esr", "current", "beta", "nightly"];

const TARGET_BROWSER_PREF = "devtools.inspector.compatibility.target-browsers";

function getDefaultTargetBrowsers() {
  // Retrieve the information that matches to the browser id and the status
  // from the browsersDataset.
  // For the structure of then browsersDataset,
  // see https://github.com/mdn/browser-compat-data/blob/master/browsers/firefox.json
  const targets = [];

  for (const id of TARGET_BROWSER_ID) {
    const { name, releases } = browsersDataset[id];

    for (const version in releases) {
      const { status } = releases[version];

      if (TARGET_BROWSER_STATUS.includes(status)) {
        targets.push({ id, name, version, status });
      }
    }
  }

  return targets;
}

function getTargetBrowsers() {
  const targetsString = Services.prefs.getCharPref(TARGET_BROWSER_PREF);
  return targetsString ? JSON.parse(targetsString) : getDefaultTargetBrowsers();
}

function setTargetBrowsers(targets) {
  Services.prefs.setCharPref(TARGET_BROWSER_PREF, JSON.stringify(targets));
}

module.exports = {
  getDefaultTargetBrowsers,
  getTargetBrowsers,
  setTargetBrowsers,
};
