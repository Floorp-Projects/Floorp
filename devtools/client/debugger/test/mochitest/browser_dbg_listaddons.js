/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the listAddons request works as specified.
 */
const ADDON1_ID = "jid1-oBAwBoE5rSecNg@jetpack";
const ADDON1_PATH = "addon1.xpi";
const ADDON2_ID = "jid1-qjtzNGV8xw5h2A@jetpack";
const ADDON2_PATH = "addon2.xpi";

var gAddon1, gAddon1Actor, gAddon2, gAddon2Actor, gClient;

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  let transport = DebuggerServer.connectPipe();
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
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testFirstAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", () => {
    addonListChanged = true;
  });

  return addTemporaryAddon(ADDON1_PATH).then(aAddon => {
    gAddon1 = aAddon;

    return getAddonActorForId(gClient, ADDON1_ID).then(aGrip => {
      ok(!addonListChanged, "Should not yet be notified that list of addons changed.");
      ok(aGrip, "Should find an addon actor for addon1.");
      gAddon1Actor = aGrip.actor;
    });
  });
}

function testSecondAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function () {
    addonListChanged = true;
  });

  return addTemporaryAddon(ADDON2_PATH).then(aAddon => {
    gAddon2 = aAddon;

    return getAddonActorForId(gClient, ADDON1_ID).then(aFirstGrip => {
      return getAddonActorForId(gClient, ADDON2_ID).then(aSecondGrip => {
        ok(addonListChanged, "Should be notified that list of addons changed.");
        is(aFirstGrip.actor, gAddon1Actor, "First addon's actor shouldn't have changed.");
        ok(aSecondGrip, "Should find a addon actor for the second addon.");
        gAddon2Actor = aSecondGrip.actor;
      });
    });
  });
}

function testRemoveFirstAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function () {
    addonListChanged = true;
  });

  return removeAddon(gAddon1).then(() => {
    return getAddonActorForId(gClient, ADDON1_ID).then(aGrip => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!aGrip, "Shouldn't find a addon actor for the first addon anymore.");
    });
  });
}

function testRemoveSecondAddon() {
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function () {
    addonListChanged = true;
  });

  return removeAddon(gAddon2).then(() => {
    return getAddonActorForId(gClient, ADDON2_ID).then(aGrip => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!aGrip, "Shouldn't find a addon actor for the second addon anymore.");
    });
  });
}

registerCleanupFunction(function () {
  gAddon1 = null;
  gAddon1Actor = null;
  gAddon2 = null;
  gAddon2Actor = null;
  gClient = null;
});
