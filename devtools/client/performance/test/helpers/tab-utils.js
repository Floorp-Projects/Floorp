/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* globals dump */

const Services = require("Services");
const tabs = require("sdk/tabs");
const tabUtils = require("sdk/tabs/utils");
const { viewFor } = require("sdk/view/core");
const { waitForDelayedStartupFinished } = require("devtools/client/performance/test/helpers/wait-utils");

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
exports.addTab = function ({ url, win }, options = {}) {
  let id = getRandomInt(0, Number.MAX_SAFE_INTEGER - 1);
  url += `#${id}`;

  dump(`Adding tab with url: ${url}.\n`);

  return new Promise(resolve => {
    let tab;

    tabs.on("ready", function onOpen(model) {
      if (tab != viewFor(model)) {
        return;
      }
      dump(`Tab added and finished loading: ${model.url}.\n`);
      tabs.off("ready", onOpen);
      resolve(tab);
    });

    win.focus();
    tab = tabUtils.openTab(win, url);

    if (options.dontWaitForTabReady) {
      resolve(tab);
    }
  });
};

/**
 * Removes a browser tab from the specified window and waits for it to close.
 */
exports.removeTab = function (tab, options = {}) {
  dump(`Removing tab: ${tabUtils.getURI(tab)}.\n`);

  return new Promise(resolve => {
    tabs.on("close", function onClose(model) {
      if (tab != viewFor(model)) {
        return;
      }
      dump(`Tab removed and finished closing: ${model.url}.\n`);
      tabs.off("close", onClose);
      resolve(tab);
    });

    tabUtils.closeTab(tab);

    if (options.dontWaitForTabClose) {
      resolve(tab);
    }
  });
};

/**
 * Adds a browser window with the provided options.
 */
exports.addWindow = function* (options) {
  let { OpenBrowserWindow } = Services.wm.getMostRecentWindow("navigator:browser");
  let win = OpenBrowserWindow(options);
  yield waitForDelayedStartupFinished(win);
  return win;
};
