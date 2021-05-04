/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that logpoints generate console messages.
 */

const Resources = require("devtools/server/actors/resources/index");

add_task(
  threadFrontTest(async ({ threadActor, threadFront, debuggee, client }) => {
    let lastMessage, lastExpression;
    const targetActor = threadActor._parent;
    // Only Workers are evaluating through the WebConsoleActor.
    // Tabs will be evaluating directly via the frame object.
    targetActor._consoleActor = {
      evaluateJS(expression) {
        lastExpression = expression;
      },
    };

    // And then listen for resource RDP event.
    // Bug 1646677: But we should probably migrate this test to ResourceCommand so that
    // we don't have to hack the server side via Resource.watchResources call.
    targetActor.on("resource-available-form", resources => {
      if (resources[0].resourceType == Resources.TYPES.CONSOLE_MESSAGE) {
        lastMessage = resources[0].message;
      }
    });

    // But both tabs and processes will be going through the ConsoleMessages module
    // We force watching for console message first,
    await Resources.watchResources(targetActor, [
      Resources.TYPES.CONSOLE_MESSAGE,
    ]);

    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet.frame.where.actor);

    // Set a logpoint which should invoke console.log.
    threadFront.setBreakpoint(
      {
        sourceUrl: source.url,
        line: 3,
      },
      { logValue: "a" }
    );
    await client.waitForRequestsToSettle();

    // Execute the rest of the code.
    await threadFront.resume();

    // NOTE: logpoints evaluated in a worker have a lastExpression
    if (lastMessage) {
      Assert.equal(lastMessage.level, "logPoint");
      Assert.equal(lastMessage.arguments[0], "three");
    } else {
      Assert.equal(lastExpression.text, "console.log(...[a])");
      Assert.equal(lastExpression.lineNumber, 3);
    }
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "debugger;\n" + // 1
    "var a = 'three';\n" + // 2
      "var b = 2;\n", // 3
    debuggee,
    "1.8",
    "test.js",
    1
  );
  /* eslint-enable */
}
