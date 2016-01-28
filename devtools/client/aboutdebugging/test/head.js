/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu, classes: Cc, interfaces: Ci} = Components;

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {AddonManager} = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const Services = require("Services");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;

const CHROME_ROOT = gTestPath.substr(0, gTestPath.lastIndexOf("/") + 1);

registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

function openAboutDebugging(page) {
  info("opening about:debugging");
  let url = "about:debugging";
  if (page) {
    url += "#" + page;
  }
  return addTab(url).then(tab => {
    let browser = tab.linkedBrowser;
    return {
      tab,
      document: browser.contentDocument,
    };
  });
}

function closeAboutDebugging(tab) {
  info("Closing about:debugging");
  return removeTab(tab);
}

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  return new Promise(done => {
    let targetWindow = aWindow || window;
    let targetBrowser = targetWindow.gBrowser;

    targetWindow.focus();
    let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onLoad() {
      linkedBrowser.removeEventListener("load", onLoad, true);
      info("Tab added and finished loading: " + aUrl);
      done(tab);
    }, true);
  });
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  return new Promise(done => {
    let targetWindow = aWindow || window;
    let targetBrowser = targetWindow.gBrowser;
    let tabContainer = targetBrowser.tabContainer;

    tabContainer.addEventListener("TabClose", function onClose(aEvent) {
      tabContainer.removeEventListener("TabClose", onClose, false);
      info("Tab removed and finished closing.");
      done();
    }, false);

    targetBrowser.removeTab(aTab);
  });
}

function get_supports_file(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
  getService(Ci.nsIChromeRegistry);
  let fileurl = cr.convertChromeURL(Services.io.newURI(CHROME_ROOT + path, null, null));
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function installAddon(document, path, evt) {
  // Mock the file picker to select a test addon
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(null);
  let file = get_supports_file(path);
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
