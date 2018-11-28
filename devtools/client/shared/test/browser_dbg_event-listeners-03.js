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

const TAB_URL = TEST_URI_ROOT + "doc_native-event-handler.html";

add_task(async () => {
  const tab = await addTab(TAB_URL);
  const { client, threadClient } = await attachThreadActorForTab(tab);

  await pauseDebuggee(client, tab, threadClient);
  await testEventListeners(threadClient);
});

function pauseDebuggee(client, tab, threadClient) {
  const deferred = getDeferredPromise().defer();

  client.addOneTimeListener("paused", (event, packet) => {
    is(packet.type, "paused",
      "We should now be paused.");
    is(packet.why.type, "debuggerStatement",
      "The debugger statement was hit.");

    deferred.resolve(threadClient);
  });

  generateMouseClickInTab(tab, "content.document.querySelector('button')");

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
