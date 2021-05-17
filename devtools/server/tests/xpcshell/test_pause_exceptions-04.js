/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

const { waitForTick } = require("devtools/shared/DevToolsUtils");

/**
 * Test that setting pauseOnExceptions to true and then to false will not cause
 * the debuggee to pause when an exception is thrown.
 */

add_task(
  threadFrontTest(
    async ({ threadFront, client, debuggee, commands }) => {
      let onResume = null;
      let packet = null;

      threadFront.once("paused", function(pkt) {
        packet = pkt;
        onResume = threadFront.resume();
      });

      await commands.threadConfigurationCommand.updateConfiguration({
        pauseOnExceptions: true,
        ignoreCaughtExceptions: true,
      });

      await evaluateTestCode(debuggee, "42");

      await onResume;

      Assert.equal(!!packet, true);
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, "42");
      packet = null;

      threadFront.once("paused", function(pkt) {
        packet = pkt;
        onResume = threadFront.resume();
      });

      await commands.threadConfigurationCommand.updateConfiguration({
        pauseOnExceptions: false,
        ignoreCaughtExceptions: true,
      });

      await evaluateTestCode(debuggee, "43");

      // Test that the paused listener callback hasn't been called
      // on the thrown error from dontStopMe()
      Assert.equal(!!packet, false);

      await commands.threadConfigurationCommand.updateConfiguration({
        pauseOnExceptions: true,
        ignoreCaughtExceptions: true,
      });

      await evaluateTestCode(debuggee, "44");

      await onResume;

      // Test that the paused listener callback has been called
      // on the thrown error from stopMeAgain()
      Assert.equal(!!packet, true);
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, "44");
    },
    {
      // Bug 1508289, exception tests fails in worker scope
      doNotRunWorker: true,
    }
  )
);

async function evaluateTestCode(debuggee, throwValue) {
  await waitForTick();
  try {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMeAgain() {            // 2
        throw ${throwValue};              // 3
      }                                   // 4
      stopMeAgain();                      // 5
      `,                                  // 6
      debuggee,
      "1.8",
      "test_pause_exceptions-04.js",
      1
    );
    /* eslint-enable */
  } catch (e) {}
}
