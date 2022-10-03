/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ThreadFront } = require("resource://devtools/client/fronts/thread.js");
const {
  WindowGlobalTargetFront,
} = require("resource://devtools/client/fronts/targets/window-global.js");

/**
 * Very naive test that checks threadClearTest helper.
 * It ensures that the thread front is correctly attached.
 */
add_task(
  threadFrontTest(({ threadFront, debuggee, client, targetFront }) => {
    ok(true, "Thread actor was able to attach");
    ok(threadFront instanceof ThreadFront, "Thread Front is valid");
    Assert.equal(threadFront.state, "attached", "Thread Front is resumed");
    Assert.equal(
      Cu.getSandboxMetadata(debuggee),
      undefined,
      "Debuggee client is valid (getSandboxMetadata did not fail)"
    );
    ok(client instanceof DevToolsClient, "Client is valid");
    ok(targetFront instanceof WindowGlobalTargetFront, "TargetFront is valid");
  })
);
