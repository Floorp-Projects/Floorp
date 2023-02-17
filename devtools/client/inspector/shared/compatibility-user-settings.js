/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const TARGET_BROWSER_ID = [
  "firefox",
  "firefox_android",
  "chrome",
  "chrome_android",
  "safari",
  "safari_ios",
  "edge",
  "ie",
];
const TARGET_BROWSER_STATUS = ["esr", "current", "beta", "nightly"];

const TARGET_BROWSER_PREF = "devtools.inspector.compatibility.target-browsers";

/**
 * Returns the full list of browsers in the RemoteSetting devtools-compatibility-browsers
 * collection (which is a flat copy of MDN compat data), sorted by browser and version.
 *
 * @returns Promise<Array<Object>> : Objects in the array have the following shape:
 *          - {string} id: The browser id (e.g. `firefox`,`safari_ios`). Should be one of TARGET_BROWSER_ID
 *          - {string} name: The browser display name (e.g. `Firefox`,`Safari for IOS`, …)
 *          - {string} version: The browser version (e.g. `99`,`15.3`, `1.0.4`, …)
 *          - {string} status: The browser status (e.g. `current`,`beta`, …). Should be one of TARGET_BROWSER_STATUS
 */
async function getBrowsersList() {
  const records = await RemoteSettings("devtools-compatibility-browsers", {
    filterFunc: record => {
      if (
        !TARGET_BROWSER_ID.includes(record.browserid) ||
        !TARGET_BROWSER_STATUS.includes(record.status)
      ) {
        return null;
      }
      return {
        id: record.browserid,
        name: record.name,
        version: record.version,
        status: record.status,
      };
    },
  }).get();

  const numericCollator = new Intl.Collator([], { numeric: true });
  records.sort((a, b) => {
    if (a.id == b.id) {
      return numericCollator.compare(a.version, b.version);
    }
    return a.id > b.id ? 1 : -1;
  });

  // MDN compat data might have browser data that have the same id and status.
  // e.g. https://github.com/mdn/browser-compat-data/commit/53453400ecb2a85e7750d99e2e0a1611648d1d56#diff-31a16f09157f13354db27821261604aa
  // In this case, only keep the newer version to keep uniqueness by id and status.
  // This needs to be done after sorting since we rely on the order of the records.
  return records.filter((record, index, arr) => {
    const nextRecord = arr[index + 1];
    // If the next record in the array is the same browser and has the same status, filter
    // out this one since it's a lower version.
    if (
      nextRecord &&
      record.id === nextRecord.id &&
      record.status === nextRecord.status
    ) {
      return false;
    }

    return true;
  });
}

/**
 * Returns the list of browsers for which we should check compatibility issues.
 *
 * @returns Promise<Array<Object>> : Objects in the array have the following shape:
 *          - {string} id: The browser id (e.g. `firefox`,`safari_ios`). Should be one of TARGET_BROWSER_ID
 *          - {string} name: The browser display name (e.g. `Firefox`,`Safari for IOS`, …)
 *          - {string} version: The browser version (e.g. `99`,`15.3`, `1.0.4`, …)
 *          - {string} status: The browser status (e.g. `current`,`beta`, …). Should be one of TARGET_BROWSER_STATUS
 */
async function getTargetBrowsers() {
  const targetsString = Services.prefs.getCharPref(TARGET_BROWSER_PREF, "");
  const browsers = await getBrowsersList();

  // If not value are stored in the pref, it means the user did not chose specific browsers,
  // so we need to return the full list.
  if (!targetsString) {
    return browsers;
  }

  const selectedBrowsersAndStatuses = JSON.parse(targetsString);
  return browsers.filter(
    browser =>
      !!selectedBrowsersAndStatuses.find(
        ({ id, status }) => browser.id == id && browser.status == status
      )
  );
}

/**
 * Store the list of browser id and status that should be used for checking compatibility
 * issues.
 *
 * @param {Object[]} browsers
 * @param {string} browsers[].id: The browser id. Should be one of TARGET_BROWSER_ID
 * @param {string} browsers[].status: The browser status. Should be one of TARGET_BROWSER_STATUS
 */
function setTargetBrowsers(browsers) {
  Services.prefs.setCharPref(
    TARGET_BROWSER_PREF,
    JSON.stringify(
      // Only store the id and the status
      browsers.map(browser => ({
        id: browser.id,
        status: browser.status,
      }))
    )
  );
}

module.exports = {
  getBrowsersList,
  getTargetBrowsers,
  setTargetBrowsers,
};
