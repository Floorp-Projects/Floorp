/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const chromeRegistry =
  Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);
const DEBUGGER_CHROME_URL = "chrome://mochitests/content/browser/devtools/client/shared/test/";
const DEBUGGER_CHROME_URI = Services.io.newURI(DEBUGGER_CHROME_URL);

var { AddonManager } = ChromeUtils.import("resource://gre/modules/AddonManager.jsm", {});

/**
 * Make sure the listAddons request works as specified.
 */
const ADDON1_ID = "jid1-oBAwBoE5rSecNg@jetpack";
const ADDON1_PATH = "addon1.xpi";
const ADDON2_ID = "jid1-qjtzNGV8xw5h2A@jetpack";
const ADDON2_PATH = "addon2.xpi";

var gAddon1, gAddon1Actor, gAddon2, gClient;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    promise.resolve(null)
      .then(testFirstAddon)
      .then(testSecondAddon)
      .then(testRemoveFirstAddon)
      .then(testRemoveSecondAddon)
      .then(() => gClient.close())
      .then(finish)
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });
  });
}

function testFirstAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", () => {
    addonListChanged = true;
  });

  return addTemporaryAddon(ADDON1_PATH).then(addon => {
    gAddon1 = addon;

    return getAddonActorForId(gClient, ADDON1_ID).then(grip => {
      ok(!addonListChanged, "Should not yet be notified that list of addons changed.");
      ok(grip, "Should find an addon actor for addon1.");
      gAddon1Actor = grip.actor;
    });
  });
}

function testSecondAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function() {
    addonListChanged = true;
  });

  return addTemporaryAddon(ADDON2_PATH).then(addon => {
    gAddon2 = addon;

    return getAddonActorForId(gClient, ADDON1_ID).then(fistGrip => {
      return getAddonActorForId(gClient, ADDON2_ID).then(secondGrip => {
        ok(addonListChanged, "Should be notified that list of addons changed.");
        is(fistGrip.actor, gAddon1Actor, "First addon's actor shouldn't have changed.");
        ok(secondGrip, "Should find a addon actor for the second addon.");
      });
    });
  });
}

function testRemoveFirstAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function() {
    addonListChanged = true;
  });

  return removeAddon(gAddon1).then(() => {
    return getAddonActorForId(gClient, ADDON1_ID).then(grip => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!grip, "Shouldn't find a addon actor for the first addon anymore.");
    });
  });
}

function testRemoveSecondAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function() {
    addonListChanged = true;
  });

  return removeAddon(gAddon2).then(() => {
    return getAddonActorForId(gClient, ADDON2_ID).then(grip => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!grip, "Shouldn't find a addon actor for the second addon anymore.");
    });
  });
}

registerCleanupFunction(function() {
  gAddon1 = null;
  gAddon1Actor = null;
  gAddon2 = null;
  gClient = null;
});

function getAddonURIFromPath(path) {
  const chromeURI = Services.io.newURI(path, null, DEBUGGER_CHROME_URI);
  return chromeRegistry.convertChromeURL(chromeURI).QueryInterface(Ci.nsIFileURL);
}

function addTemporaryAddon(path) {
  const addonFile = getAddonURIFromPath(path).file;
  info("Installing addon: " + addonFile.path);

  return AddonManager.installTemporaryAddon(addonFile);
}

function getAddonActorForId(client, addonId) {
  info("Get addon actor for ID: " + addonId);
  const deferred = promise.defer();

  client.listAddons().then(response => {
    const addonTargetActor = response.addons.filter(grip => grip.id == addonId).pop();
    info("got addon actor for ID: " + addonId);
    deferred.resolve(addonTargetActor);
  });

  return deferred.promise;
}

function removeAddon(addon) {
  info("Removing addon.");

  const deferred = promise.defer();

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
