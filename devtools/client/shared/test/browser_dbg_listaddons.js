/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_addons.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_addons.js",
  this);

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

/**
 * Make sure the listAddons request works as specified.
 */
const ADDON1_ID = "jid1-oBAwBoE5rSecNg@jetpack";
const ADDON1_PATH = "addon1.xpi";
const ADDON2_ID = "jid1-qjtzNGV8xw5h2A@jetpack";
const ADDON2_PATH = "addon2.xpi";

var gAddon1, gAddon1Front, gAddon2, gClient;

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
  gClient.mainRoot.once("addonListChanged").then(() => {
    addonListChanged = true;
  });

  return addTemporaryAddon(ADDON1_PATH).then(addon => {
    gAddon1 = addon;

    return gClient.mainRoot.getAddon({ id: ADDON1_ID }).then(front => {
      ok(!addonListChanged, "Should not yet be notified that list of addons changed.");
      ok(front, "Should find an addon actor for addon1.");
      gAddon1Front = front;
    });
  });
}

function testSecondAddon() {
  let addonListChanged = false;
  gClient.mainRoot.once("addonListChanged").then(() => {
    addonListChanged = true;
  });

  return addTemporaryAddon(ADDON2_PATH).then(addon => {
    gAddon2 = addon;

    return gClient.mainRoot.getAddon({ id: ADDON1_ID }).then(front1 => {
      return gClient.mainRoot.getAddon({ id: ADDON2_ID }).then(front2 => {
        ok(addonListChanged, "Should be notified that list of addons changed.");
        is(front1, gAddon1Front, "First addon's actor shouldn't have changed.");
        ok(front2, "Should find a addon actor for the second addon.");
      });
    });
  });
}

function testRemoveFirstAddon() {
  let addonListChanged = false;
  gClient.mainRoot.once("addonListChanged").then(() => {
    addonListChanged = true;
  });

  return removeAddon(gAddon1).then(() => {
    return gClient.mainRoot.getAddon({ id: ADDON1_ID }).then(front => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!front, "Shouldn't find a addon actor for the first addon anymore.");
    });
  });
}

function testRemoveSecondAddon() {
  let addonListChanged = false;
  gClient.mainRoot.once("addonListChanged").then(() => {
    addonListChanged = true;
  });

  return removeAddon(gAddon2).then(() => {
    return gClient.mainRoot.getAddon({ id: ADDON2_ID }).then(front => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!front, "Shouldn't find a addon actor for the second addon anymore.");
    });
  });
}

registerCleanupFunction(function() {
  gAddon1 = null;
  gAddon1Front = null;
  gAddon2 = null;
  gClient = null;
});
