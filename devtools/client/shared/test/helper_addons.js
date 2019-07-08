/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

const chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
  Ci.nsIChromeRegistry
);
const DEBUGGER_CHROME_URL =
  "chrome://mochitests/content/browser/devtools/client/shared/test/";
const DEBUGGER_CHROME_URI = Services.io.newURI(DEBUGGER_CHROME_URL);

const EventEmitter = require("devtools/shared/event-emitter");

var { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

/**
 * Returns a thenable promise
 * @return {Promise}
 */
function getDeferredPromise() {
  // Override promise with deprecated-sync-thenables
  const promise = require("devtools/shared/deprecated-sync-thenables");
  return promise;
}

function getAddonURIFromPath(path) {
  const chromeURI = Services.io.newURI(path, null, DEBUGGER_CHROME_URI);
  return chromeRegistry
    .convertChromeURL(chromeURI)
    .QueryInterface(Ci.nsIFileURL);
}

function addTemporaryAddon(path) {
  const addonFile = getAddonURIFromPath(path).file;
  info("Installing addon: " + addonFile.path);

  return AddonManager.installTemporaryAddon(addonFile);
}

function removeAddon(addon) {
  info("Removing addon.");

  const deferred = getDeferredPromise().defer();

  const listener = {
    onUninstalled: function(uninstalledAddon) {
      if (uninstalledAddon != addon) {
        return;
      }
      AddonManager.removeAddonListener(listener);
      deferred.resolve();
    },
  };
  AddonManager.addAddonListener(listener);
  addon.uninstall();

  return deferred.promise;
}
