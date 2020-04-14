/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure the listTabs request works as specified.
 */

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
const TAB2_URL = EXAMPLE_URL + "doc_empty-tab-02.html";

var gTab1, gTab1Front, gTab2, gTab2Front, gClient;

function test() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();
  gClient = new DevToolsClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser", "Root actor should identify itself as a browser.");

    promise
      .resolve(null)
      .then(testFirstTab)
      .then(testSecondTab)
      .then(testRemoveTab)
      .then(testAttachRemovedTab)
      .then(() => gClient.close())
      .then(finish)
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });
  });
}

function testFirstTab() {
  return addTab(TAB1_URL).then(tab => {
    gTab1 = tab;

    return getTargetActorForUrl(gClient, TAB1_URL).then(front => {
      ok(front, "Should find a target actor for the first tab.");
      gTab1Front = front;
    });
  });
}

function testSecondTab() {
  return addTab(TAB2_URL).then(tab => {
    gTab2 = tab;

    return getTargetActorForUrl(gClient, TAB1_URL).then(firstFront => {
      return getTargetActorForUrl(gClient, TAB2_URL).then(secondFront => {
        is(firstFront, gTab1Front, "First tab's actor shouldn't have changed.");
        ok(secondFront, "Should find a target actor for the second tab.");
        gTab2Front = secondFront;
      });
    });
  });
}

function testRemoveTab() {
  return removeTab(gTab1).then(() => {
    return getTargetActorForUrl(gClient, TAB1_URL).then(front => {
      ok(!front, "Shouldn't find a target actor for the first tab anymore.");
    });
  });
}

function testAttachRemovedTab() {
  return removeTab(gTab2).then(() => {
    const deferred = promise.defer();

    gClient.on("paused", () => {
      ok(
        false,
        "Attaching to an exited target actor shouldn't generate a pause."
      );
      deferred.reject();
    });

    gTab2Front.reconfigure({}).then(null, error => {
      ok(
        error.message.includes("noSuchActor"),
        "Actor is gone since the tab was removed."
      );
      deferred.resolve();
    });

    return deferred.promise;
  });
}

registerCleanupFunction(function() {
  gTab1 = null;
  gTab1Front = null;
  gTab2 = null;
  gTab2Front = null;
  gClient = null;
});

async function getTargetActorForUrl(client, url) {
  const tabDescriptors = await client.mainRoot.listTabs();
  const tabDescriptor = tabDescriptors.find(front => front.url == url);
  return tabDescriptor?.getTarget();
}
