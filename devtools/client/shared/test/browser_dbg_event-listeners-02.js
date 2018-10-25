/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the eventListeners request works when bound functions are used as
 * event listeners.
 */

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this);

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const TAB_URL = TEST_URI_ROOT + "doc_event-listeners-03.html";

add_task(async function() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  const [type] = await client.connect();

  Assert.equal(type, "browser",
    "Root actor should identify itself as a browser.");

  const tab = await addTab(TAB_URL);
  const threadClient = await attachThreadActorForUrl(client, TAB_URL);
  await pauseDebuggee(tab, client, threadClient);
  await testEventListeners(client, threadClient);
  await client.close();
});

function pauseDebuggee(tab, client, threadClient) {
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

async function testEventListeners(client, threadClient) {
  const packet = await threadClient.eventListeners();

  if (packet.error) {
    const msg = "Error getting event listeners: " + packet.message;
    ok(false, msg);
    return;
  }

  is(packet.listeners.length, 3,
    "Found all event listeners.");

  const listeners = await getDeferredPromise().all(packet.listeners.map(listener => {
    const lDeferred = getDeferredPromise().defer();
    threadClient.pauseGrip(listener.function).getDefinitionSite(response => {
      if (response.error) {
        const msg = "Error getting function definition site: " + response.message;
        ok(false, msg);
        lDeferred.reject(msg);
        return;
      }
      listener.function.url = response.source.url;
      lDeferred.resolve(listener);
    });
    return lDeferred.promise;
  }));

  Assert.equal(listeners.length, 3, "Found three event listeners.");

  for (const l of listeners) {
    const node = l.node;
    ok(node, "There is a node property.");
    ok(node.object, "There is a node object property.");

    if (node.selector != "window") {
      const nodeCount =
        await ContentTask.spawn(gBrowser.selectedBrowser, node.selector,
          async (selector) => {
            return content.document.querySelectorAll(selector).length;
          });
      Assert.equal(nodeCount, 1, "The node property is a unique CSS selector.");
    } else {
      Assert.ok(true, "The node property is a unique CSS selector.");
    }

    const func = l.function;
    ok(func, "There is a function property.");
    is(func.type, "object", "The function form is of type 'object'.");
    is(func.class, "Function", "The function form is of class 'Function'.");
    is(func.url, TAB_URL, "The function url is correct.");

    is(l.type, "click", "This is a click event listener.");
    is(l.allowsUntrusted, true,
      "'allowsUntrusted' property has the right value.");
    is(l.inSystemEventGroup, false,
      "'inSystemEventGroup' property has the right value.");
    is(l.isEventHandler, false,
      "'isEventHandler' property has the right value.");
    is(l.capturing, false,
      "Capturing property has the right value.");
  }

  await threadClient.resume();
}
