/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ThreadClient } = require("devtools/shared/client/thread-client");
const {
  BrowsingContextTargetFront,
} = require("devtools/shared/fronts/targets/browsing-context");

/**
 * Very naive test that checks threadClearTest helper.
 * It ensures that the thread client is correctly attached.
 */
add_task(
  threadClientTest(({ threadClient, debuggee, client, targetFront }) => {
    ok(true, "Thread actor was able to attach");
    ok(threadClient instanceof ThreadClient, "Thread client is valid");
    Assert.equal(threadClient.state, "attached", "Thread client is resumed");
    Assert.equal(
      String(debuggee),
      "[object Sandbox]",
      "Debuggee client is valid"
    );
    ok(client instanceof DebuggerClient, "Client is valid");
    ok(
      targetFront instanceof BrowsingContextTargetFront,
      "TargetFront is valid"
    );
  })
);
