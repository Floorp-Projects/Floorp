/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* globals dump */

const { BrowserTestUtils } = require("resource://testing-common/BrowserTestUtils.jsm");
const Services = require("Services");
const { waitForDelayedStartupFinished } = require("devtools/client/performance/test/helpers/wait-utils");
const { gDevTools } = require("devtools/client/framework/devtools");

/**
 * Gets a random integer in between an interval. Used to uniquely identify
 * added tabs by augmenting the URL.
 */
function getRandomInt(min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

/**
 * Adds a browser tab with the given url in the specified window and waits
 * for it to load.
 */
exports.addTab = function({ url, win }, options = {}) {
  const id = getRandomInt(0, Number.MAX_SAFE_INTEGER - 1);
  url += `#${id}`;

  dump(`Adding tab with url: ${url}.\n`);

  const { gBrowser } = win || window;
  return BrowserTestUtils.openNewForegroundTab(gBrowser, url,
                                               !options.dontWaitForTabReady);
};

/**
 * Removes a browser tab from the specified window and waits for it to close.
 */
exports.removeTab = function(tab) {
  dump(`Removing tab: ${tab.linkedBrowser.currentURI.spec}.\n`);

  BrowserTestUtils.removeTab(tab);
};

/**
 * Adds a browser window with the provided options.
 */
exports.addWindow = async function(options) {
  const { OpenBrowserWindow } =
    Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  const win = OpenBrowserWindow(options);
  await waitForDelayedStartupFinished(win);
  return win;
};
