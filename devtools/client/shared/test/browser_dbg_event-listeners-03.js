/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the eventListeners request works when there are event handlers
 * that the debugger cannot unwrap.
 */

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this);

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const TAB_URL = TEST_URI_ROOT + "doc_native-event-handler.html";

var gClient;
var gTab;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then((tab) => {
        gTab = tab;
        return attachThreadActorForUrl(gClient, TAB_URL);
      })
      .then(pauseDebuggee)
      .then(testEventListeners)
      .then(() => gClient.close())
      .then(finish)
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });
  });
}

function pauseDebuggee(threadClient) {
  const deferred = getDeferredPromise().defer();

  gClient.addOneTimeListener("paused", (event, packet) => {
    is(packet.type, "paused",
      "We should now be paused.");
    is(packet.why.type, "debuggerStatement",
      "The debugger statement was hit.");

    deferred.resolve(threadClient);
  });

  generateMouseClickInTab(gTab, "content.document.querySelector('button')");

  return deferred.promise;
}

function testEventListeners(threadClient) {
  const deferred = getDeferredPromise().defer();

  threadClient.eventListeners(packet => {
    if (packet.error) {
      const msg = "Error getting event listeners: " + packet.message;
      ok(false, msg);
      deferred.reject(msg);
      return;
    }

    // There are 2 event listeners in the page: button.onclick, window.onload.
    // The video element controls listeners are skipped — they cannot be
    // unwrapped but they shouldn't cause us to throw either.
    is(packet.listeners.length, 2, "Found all event listeners.");
    threadClient.resume(deferred.resolve);
  });

  return deferred.promise;
}

registerCleanupFunction(function() {
  gClient = null;
});
