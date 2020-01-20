/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting pauseOnExceptions to true will cause the debuggee to pause
 * when an exceptions is thrown.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_pause_frame();
    },
    { waitForFinish: true }
  )
);

function test_pause_frame() {
  gThreadFront.once("paused", function(packet) {
    gThreadFront.once("paused", function(packet) {
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, 42);
      gThreadFront.resume().then(() => threadFrontTestFinished());
    });

    gThreadFront.pauseOnExceptions(true, false);
    gThreadFront.resume();
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe() {
      debugger;
      throw 42;
    }
    try {
      stopMe();
    } catch (e) {}
  } + ")()");
  /* eslint-enable */
}
