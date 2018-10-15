/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check tab attach/navigation.
 */

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
const TAB2_FILE = "doc_empty-tab-02.html";
const TAB2_URL = EXAMPLE_URL + TAB2_FILE;

var gClient;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB1_URL)
      .then(() => attachTargetActorForUrl(gClient, TAB1_URL))
      .then(testNavigate)
      .then(testDetach)
      .then(finish)
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });
  });
}

function testNavigate(targetFront) {
  const outstanding = [promise.defer(), promise.defer()];

  targetFront.on("tabNavigated", function onTabNavigated(packet) {
    is(packet.url.split("/").pop(), TAB2_FILE,
      "Got a tab navigation notification.");

    info(JSON.stringify(packet));
    info(JSON.stringify(event));

    if (packet.state == "start") {
      ok(true, "Tab started to navigate.");
      outstanding[0].resolve();
    } else {
      ok(true, "Tab finished navigating.");
      targetFront.off("tabNavigated", onTabNavigated);
      outstanding[1].resolve();
    }
  });

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TAB2_URL);
  return promise.all(outstanding.map(e => e.promise))
                .then(() => targetFront);
}

async function testDetach(targetFront) {
  const onDetached = targetFront.once("tabDetached");

  removeTab(gBrowser.selectedTab);

  const packet = await onDetached;
  ok(true, "Got a tab detach notification.");
  is(packet.from, targetFront.actorID,
    "tab detach message comes from the expected actor");

  return gClient.close();
}

registerCleanupFunction(function() {
  gClient = null;
});

async function attachTargetActorForUrl(client, url) {
  const grip = await getTargetActorForUrl(client, url);
  const [, targetFront] = await client.attachTarget(grip.actor);
  return targetFront;
}

function getTargetActorForUrl(client, url) {
  const deferred = promise.defer();

  client.listTabs().then(response => {
    const targetActor = response.tabs.filter(grip => grip.url == url).pop();
    deferred.resolve(targetActor);
  });

  return deferred.promise;
}
