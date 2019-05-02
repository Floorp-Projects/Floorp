/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test exceptions inside black boxed sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(
      gClient, "test-black-box",
      function(response, targetFront, threadClient) {
        gThreadClient = threadClient;
        // XXX: We have to do an executeSoon so that the error isn't caught and
        // reported by DebuggerClient.requester (because we are using the local
        // transport and share a stack) which causes the test to fail.
        Services.tm.dispatchToMainThread({
          run: test_black_box,
        });
      });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function test_black_box() {
  gClient.addOneTimeListener("paused", test_black_box_exception);

  /* eslint-disable no-multi-spaces, no-unreachable, no-undef */
  Cu.evalInSandbox(
    "" + function doStuff(k) {                                   // line 1
      throw new Error("error msg");                              // line 2
      k(100);                                                    // line 3
    },                                                           // line 4
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Cu.evalInSandbox(
    "" + function runTest() {                   // line 1
      doStuff(                                  // line 2
        function(n) {                          // line 3
          debugger;                             // line 4
        }                                       // line 5
      );                                        // line 6
    }                                           // line 7
    + "\ndebugger;\n"                           // line 8
    + "try { runTest() } catch (ex) { }",       // line 9
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces, no-unreachable, no-undef */
}

function test_black_box_exception() {
  gThreadClient.getSources().then(async function({error, sources}) {
    Assert.ok(!error, "Should not get an error: " + error);
    const sourceFront = await getSource(gThreadClient, BLACK_BOXED_URL);
    await blackBox(sourceFront);
    gThreadClient.pauseOnExceptions(true, false);

    gClient.addOneTimeListener("paused", async function(event, packet) {
      const source = await getSourceById(gThreadClient, packet.frame.where.actor);

      Assert.equal(source.url, SOURCE_URL,
                   "We shouldn't pause while in the black boxed source.");
      finishClient(gClient);
    });

    gThreadClient.resume();
  });
}
