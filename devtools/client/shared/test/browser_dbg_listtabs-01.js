/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure the listTabs request works as specified.
 */

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
const TAB2_URL = EXAMPLE_URL + "doc_empty-tab-02.html";

var gTab1, gTab1Actor, gTab2, gTab2Actor, gClient;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
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
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });
  });
}

function testFirstTab() {
  return addTab(TAB1_URL).then(tab => {
    gTab1 = tab;

    return getTargetActorForUrl(gClient, TAB1_URL).then(form => {
      ok(form, "Should find a target actor for the first tab.");
      gTab1Actor = form.actor;
    });
  });
}

function testSecondTab() {
  return addTab(TAB2_URL).then(tab => {
    gTab2 = tab;

    return getTargetActorForUrl(gClient, TAB1_URL).then(firstGrip => {
      return getTargetActorForUrl(gClient, TAB2_URL).then(secondGrip => {
        is(firstGrip.actor, gTab1Actor, "First tab's actor shouldn't have changed.");
        ok(secondGrip, "Should find a target actor for the second tab.");
        gTab2Actor = secondGrip.actor;
      });
    });
  });
}

function testRemoveTab() {
  return removeTab(gTab1).then(() => {
    return getTargetActorForUrl(gClient, TAB1_URL).then(form => {
      ok(!form, "Shouldn't find a target actor for the first tab anymore.");
    });
  });
}

function testAttachRemovedTab() {
  return removeTab(gTab2).then(() => {
    const deferred = promise.defer();

    gClient.addListener("paused", () => {
      ok(false, "Attaching to an exited target actor shouldn't generate a pause.");
      deferred.reject();
    });

    gClient.request({ to: gTab2Actor, type: "attach" }, response => {
      is(response.error, "connectionClosed",
         "Connection is gone since the tab was removed.");
      deferred.resolve();
    });

    return deferred.promise;
  });
}

registerCleanupFunction(function() {
  gTab1 = null;
  gTab1Actor = null;
  gTab2 = null;
  gTab2Actor = null;
  gClient = null;
});

async function getTargetActorForUrl(client, url) {
  const { tabs } = await client.listTabs();
  const targetActor = tabs.filter(form => form.url == url).pop();
  return targetActor;
}
