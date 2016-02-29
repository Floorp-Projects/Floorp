/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint-disable mozilla/no-cpows-in-tests */
/* exported openAboutDebugging, closeAboutDebugging, installAddon,
   uninstallAddon, waitForMutation, assertHasTarget,
   waitForServiceWorkerRegistered, unregisterServiceWorker */
/* global sendAsyncMessage */

"use strict";

var { utils: Cu, classes: Cc, interfaces: Ci } = Components;

const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const Services = require("Services");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;

const CHROME_ROOT = gTestPath.substr(0, gTestPath.lastIndexOf("/") + 1);

registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

function* openAboutDebugging(page) {
  info("opening about:debugging");
  let url = "about:debugging";
  if (page) {
    url += "#" + page;
  }

  let tab = yield addTab(url);
  let browser = tab.linkedBrowser;
  let document = browser.contentDocument;

  if (!document.querySelector(".app")) {
    yield waitForMutation(document.body, { childList: true });
  }

  return { tab, document };
}

function closeAboutDebugging(tab) {
  info("Closing about:debugging");
  return removeTab(tab);
}

function addTab(url, win) {
  info("Adding tab: " + url);

  return new Promise(done => {
    let targetWindow = win || window;
    let targetBrowser = targetWindow.gBrowser;

    targetWindow.focus();
    let tab = targetBrowser.selectedTab = targetBrowser.addTab(url);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onLoad() {
      linkedBrowser.removeEventListener("load", onLoad, true);
      info("Tab added and finished loading: " + url);
      done(tab);
    }, true);
  });
}

function removeTab(tab, win) {
  info("Removing tab.");

  return new Promise(done => {
    let targetWindow = win || window;
    let targetBrowser = targetWindow.gBrowser;
    let tabContainer = targetBrowser.tabContainer;

    tabContainer.addEventListener("TabClose", function onClose() {
      tabContainer.removeEventListener("TabClose", onClose, false);
      info("Tab removed and finished closing.");
      done();
    }, false);

    targetBrowser.removeTab(tab);
  });
}

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  let uri = Services.io.newURI(CHROME_ROOT + path, null, null);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function installAddon(document, path, evt) {
  // Mock the file picker to select a test addon
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(null);
  let file = getSupportsFile(path);
  MockFilePicker.returnFiles = [file.file];

  // Wait for a message sent by the addon's bootstrap.js file
  let onAddonInstalled = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, evt, false);
      ok(true, "Addon installed and running its bootstrap.js file");
      done();
    }, evt, false);
  });
  // Trigger the file picker by clicking on the button
  document.getElementById("load-addon-from-file").click();

  // Wait for the addon execution
  return onAddonInstalled;
}

function uninstallAddon(addonId) {
  // Now uninstall this addon
  return new Promise(done => {
    AddonManager.getAddonByID(addonId, addon => {
      let listener = {
        onUninstalled: function(uninstalledAddon) {
          if (uninstalledAddon != addon) {
            return;
          }
          AddonManager.removeAddonListener(listener);
          done();
        }
      };
      AddonManager.addAddonListener(listener);
      addon.uninstall();
    });
  });
}

/**
 * Returns a promise that will resolve after receiving a mutation matching the
 * provided mutation options on the provided target.
 * @param {Node} target
 * @param {Object} mutationOptions
 * @return {Promise}
 */
function waitForMutation(target, mutationOptions) {
  return new Promise(resolve => {
    let observer = new MutationObserver(() => {
      observer.disconnect();
      resolve();
    });
    observer.observe(target, mutationOptions);
  });
}

/**
 * Checks if an about:debugging TargetList element contains a Target element
 * corresponding to the specified name.
 * @param {Boolean} expected
 * @param {Document} document
 * @param {String} type
 * @param {String} name
 */
function assertHasTarget(expected, document, type, name) {
  let names = [...document.querySelectorAll("#" + type + " .target-name")];
  names = names.map(element => element.textContent);
  is(names.includes(name), expected,
    "The " + type + " url appears in the list: " + names);
}

/**
 * Returns a promise that will resolve after the service worker in the page
 * has successfully registered itself.
 * @param {Tab} tab
 */
function waitForServiceWorkerRegistered(tab) {
  // Make the test page notify us when the service worker is registered.
  let frameScript = function() {
    // Retrieve the `sw` promise created in the html page.
    let { sw } = content.wrappedJSObject;
    sw.then(function(registration) {
      sendAsyncMessage("sw-registered");
    });
  };
  let mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);

  return new Promise(done => {
    mm.addMessageListener("sw-registered", function listener() {
      mm.removeMessageListener("sw-registered", listener);
      done();
    });
  });
}

/**
 * Asks the service worker within the test page to unregister, and returns a
 * promise that will resolve when it has successfully unregistered itself.
 * @param {Tab} tab
 */
function unregisterServiceWorker(tab) {
  // Use message manager to work with e10s.
  let frameScript = function() {
    // Retrieve the `sw` promise created in the html page.
    let { sw } = content.wrappedJSObject;
    sw.then(function(registration) {
      registration.unregister().then(function() {
        sendAsyncMessage("sw-unregistered");
      },
      function(e) {
        dump("SW not unregistered; " + e + "\n");
      });
    });
  };
  let mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);

  return new Promise(done => {
    mm.addMessageListener("sw-unregistered", function listener() {
      mm.removeMessageListener("sw-unregistered", listener);
      done();
    });
  });
}
