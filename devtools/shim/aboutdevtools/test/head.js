/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

const { utils: Cu } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// All test are asynchronous
waitForExplicitFinish();

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
const waitUntil = function (predicate, interval = 100) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function () {
      waitUntil(predicate, interval).then(() => resolve(true));
    }, interval);
  });
};

/**
 * Open the provided url in a new tab.
 */
const addTab = async function (url) {
  info("Adding a new tab with URL: " + url);

  let { gBrowser } = window;

  let tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("Tab added and finished loading");

  return tab;
};

/**
 * Remove the given tab.
 * @param {Object} tab The tab to be removed.
 * @return Promise<undefined> resolved when the tab is successfully removed.
 */
const removeTab = async function (tab) {
  info("Removing tab.");

  let { gBrowser } = tab.ownerGlobal;

  await new Promise(resolve => {
    gBrowser.tabContainer.addEventListener("TabClose", resolve, {once: true});
    gBrowser.removeTab(tab);
  });

  info("Tab removed and finished closing");
};

/**
 * Open a new tab on about:devtools
 */
const openAboutDevTools = async function () {
  info("Open about:devtools programmatically in a new tab");
  let tab = await addTab("about:devtools");

  let browser = tab.linkedBrowser;
  let doc = browser.contentDocument;
  let win = browser.contentWindow;

  return {tab, doc, win};
};

/**
 * Copied from devtools shared-head.js.
 * Set a temporary value for a preference, that will be cleaned up after the test.
 */
const pushPref = function (preferenceName, value) {
  return new Promise(resolve => {
    let options = {"set": [[preferenceName, value]]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });
};

/**
 * Helper to call the toggle devtools shortcut.
 */
function synthesizeToggleToolboxKey() {
  info("Trigger the toogle toolbox shortcut");
  if (Services.appinfo.OS == "Darwin") {
    EventUtils.synthesizeKey("i", { accelKey: true, altKey: true });
  } else {
    EventUtils.synthesizeKey("i", { accelKey: true, shiftKey: true });
  }
}

/**
 * Helper to check if a given tab is about:devtools.
 */
function isAboutDevtoolsTab(tab) {
  let browser = tab.linkedBrowser;
  let location = browser.documentURI.spec;
  return location.startsWith("about:devtools");
}
