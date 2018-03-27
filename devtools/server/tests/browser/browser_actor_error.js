/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clients can catch errors in old style actors.
 */

const ACTORS_URL = "chrome://mochitests/content/browser/devtools/server/tests/browser/error-actor.js";

async function test() {
  let gClient;

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  DebuggerServer.addActors(ACTORS_URL);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  await gClient.connect();

  let { errorActor } = await gClient.listTabs();
  ok(errorActor, "Found the error actor.");

  try {
    await gClient.request({ to: globalActor, type: "error" });
    ok(false, "The request is expected to fail.");
  } catch (e) {
    ok(true, "The request failed as expected, and was caught by the client");
  }
  await gClient.close();

  finish();
}
