/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the listTabs request works as specified.
 */

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
const TAB2_URL = EXAMPLE_URL + "doc_empty-tab-02.html";

var gTab1, gTab1Actor, gTab2, gTab2Actor, gClient;

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
      .then(testFirstTab)
      .then(testSecondTab)
      .then(testRemoveTab)
      .then(testAttachRemovedTab)
      .then(() => gClient.close())
      .then(finish)
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testFirstTab() {
  return addTab(TAB1_URL).then(aTab => {
    gTab1 = aTab;

    return getTabActorForUrl(gClient, TAB1_URL).then(aGrip => {
      ok(aGrip, "Should find a tab actor for the first tab.");
      gTab1Actor = aGrip.actor;
    });
  });
}

function testSecondTab() {
  return addTab(TAB2_URL).then(aTab => {
    gTab2 = aTab;

    return getTabActorForUrl(gClient, TAB1_URL).then(aFirstGrip => {
      return getTabActorForUrl(gClient, TAB2_URL).then(aSecondGrip => {
        is(aFirstGrip.actor, gTab1Actor, "First tab's actor shouldn't have changed.");
        ok(aSecondGrip, "Should find a tab actor for the second tab.");
        gTab2Actor = aSecondGrip.actor;
      });
    });
  });
}

function testRemoveTab() {
  return removeTab(gTab1).then(() => {
    return getTabActorForUrl(gClient, TAB1_URL).then(aGrip => {
      ok(!aGrip, "Shouldn't find a tab actor for the first tab anymore.");
    });
  });
}

function testAttachRemovedTab() {
  return removeTab(gTab2).then(() => {
    let deferred = promise.defer();

    gClient.addListener("paused", (aEvent, aPacket) => {
      ok(false, "Attaching to an exited tab actor shouldn't generate a pause.");
      deferred.reject();
    });

    gClient.request({ to: gTab2Actor, type: "attach" }, aResponse => {
      is(aResponse.error, "connectionClosed",
         "Connection is gone since the tab was removed.");
      deferred.resolve();
    });

    return deferred.promise;
  });
}

registerCleanupFunction(function () {
  gTab1 = null;
  gTab1Actor = null;
  gTab2 = null;
  gTab2Actor = null;
  gClient = null;
});
