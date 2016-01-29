/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the listAddons request works as specified.
 */
const ADDON1_URL = EXAMPLE_URL + "addon1.xpi";
const ADDON2_URL = EXAMPLE_URL + "addon2.xpi";

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
      .then(closeConnection)
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

  return addAddon(ADDON1_URL).then(aAddon => {
    gAddon1 = aAddon;

    return getAddonActorForUrl(gClient, ADDON1_URL).then(aGrip => {
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

  return addAddon(ADDON2_URL).then(aAddon => {
    gAddon2 = aAddon;

    return getAddonActorForUrl(gClient, ADDON1_URL).then(aFirstGrip => {
      return getAddonActorForUrl(gClient, ADDON2_URL).then(aSecondGrip => {
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

  removeAddon(gAddon1).then(() => {
    return getAddonActorForUrl(gClient, ADDON1_URL).then(aGrip => {
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

  removeAddon(gAddon2).then(() => {
    return getAddonActorForUrl(gClient, ADDON2_URL).then(aGrip => {
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!aGrip, "Shouldn't find a addon actor for the second addon anymore.");
    });
  });
}

function closeConnection() {
  let deferred = promise.defer();
  gClient.close(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function() {
  gAddon1 = null;
  gAddon1Actor = null;
  gAddon2 = null;
  gAddon2Actor = null;
  gClient = null;
});
