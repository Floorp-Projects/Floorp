/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

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
const waitUntil = function(predicate, interval = 100) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function() {
      waitUntil(predicate, interval).then(() => resolve(true));
    }, interval);
  });
};

/**
 * Copied from devtools shared-head.js
 *
 * @param {BrowsingContext} context
 **/
const waitForPresShell = function(context) {
  return SpecialPowers.spawn(context, [], async () => {
    const winUtils = SpecialPowers.getDOMWindowUtils(content);
    await ContentTaskUtils.waitForCondition(() => {
      try {
        return !!winUtils.getPresShellId();
      } catch (e) {
        return false;
      }
    }, "Waiting for a valid presShell");
  });
};

/**
 * Open the provided url in a new tab.
 */
const addTab = async function(url) {
  info("Adding a new tab with URL: " + url);

  const { gBrowser } = window;

  const tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Wait for a valid presShell to avoid test timeouts on linux webrender
  // platforms.
  await waitForPresShell(tab.linkedBrowser);

  info("Tab added and finished loading");

  return tab;
};

/**
 * Remove the given tab.
 * @param {Object} tab The tab to be removed.
 * @return Promise<undefined> resolved when the tab is successfully removed.
 */
const removeTab = async function(tab) {
  info("Removing tab.");

  const { gBrowser } = tab.ownerGlobal;

  await new Promise(resolve => {
    gBrowser.tabContainer.addEventListener("TabClose", resolve, { once: true });
    gBrowser.removeTab(tab);
  });

  info("Tab removed and finished closing");
};

/**
 * Open a new tab on about:devtools
 */
const openAboutDevTools = async function() {
  info("Open about:devtools programmatically in a new tab");
  const tab = await addTab("about:devtools");

  const browser = tab.linkedBrowser;
  const doc = browser.contentDocument;
  const win = browser.contentWindow;

  return { tab, doc, win };
};

/**
 * Copied from devtools shared-head.js.
 * Set a temporary value for a preference, that will be cleaned up after the test.
 */
const pushPref = function(preferenceName, value) {
  return new Promise(resolve => {
    const options = { set: [[preferenceName, value]] };
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
  const browser = tab.linkedBrowser;
  // browser.documentURI might be unavailable if the tab is loading.
  if (browser && browser.documentURI) {
    const location = browser.documentURI.spec;
    return location.startsWith("about:devtools");
  }
  return false;
}
