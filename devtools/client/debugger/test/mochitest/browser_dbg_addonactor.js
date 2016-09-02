/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure we can attach to addon actors.

const ADDON3_PATH = "addon3.xpi";
const ADDON3_ID = "jid1-ami3akps3baaeg@jetpack";
const ADDON_MODULE_URL = "resource://jid1-ami3akps3baaeg-at-jetpack/browser_dbg_addon3/lib/main.js";

var gAddon, gClient, gThreadClient;

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

    installAddon()
      .then(attachAddonActorForId.bind(null, gClient, ADDON3_ID))
      .then(attachAddonThread)
      .then(testDebugger)
      .then(testSources)
      .then(() => gClient.close())
      .then(uninstallAddon)
      .then(finish)
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function installAddon() {
  return addTemporaryAddon(ADDON3_PATH).then(aAddon => {
    gAddon = aAddon;
  });
}

function attachAddonThread([aGrip, aResponse]) {
  info("attached addon actor for Addon ID");
  let deferred = promise.defer();

  gClient.attachThread(aResponse.threadActor, (aResponse, aThreadClient) => {
    info("attached thread");
    gThreadClient = aThreadClient;
    gThreadClient.resume(deferred.resolve);
  });
  return deferred.promise;
}

function testDebugger() {
  info("Entering testDebugger");
  let deferred = promise.defer();

  once(gClient, "paused").then(() => {
    ok(true, "Should be able to attach to addon actor");
    gThreadClient.resume(deferred.resolve);
  });

  Services.obs.notifyObservers(null, "debuggerAttached", null);

  return deferred.promise;
}

function testSources() {
  let deferred = promise.defer();

  gThreadClient.getSources(aResponse => {
    // source URLs contain launch-specific temporary directory path,
    // hence the ".contains" call.
    const matches = aResponse.sources.filter(s => s.url.includes(ADDON_MODULE_URL));
    ok(matches.length > 0,
       "the main script of the addon is present in the source list");
    deferred.resolve();
  });

  return deferred.promise;
}

function uninstallAddon() {
  return removeAddon(gAddon);
}

registerCleanupFunction(function () {
  gClient = null;
  gAddon = null;
  gThreadClient = null;
});
