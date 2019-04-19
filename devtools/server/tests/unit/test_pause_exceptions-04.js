/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting pauseOnExceptions to true and then to false will not cause
 * the debuggee to pause when an exception is thrown.
 */

add_task(threadClientTest(async ({ threadClient, client, debuggee }) => {
  let packet = null;

  threadClient.addOneTimeListener("paused", function(event, pkt) {
    packet = pkt;
    threadClient.resume();
  });

  await threadClient.pauseOnExceptions(true, true);
  try {
    /* eslint-disable */
    debuggee.eval(
      "(" + function() {
        function stopMe() {
          throw 42;
        }
        stopMe();
      } + ")()"
    );
    /* eslint-enable */
  } catch (e) {}

  Assert.equal(!!packet, true);
  Assert.equal(packet.why.type, "exception");
  Assert.equal(packet.why.exception, "42");
  packet = null;

  threadClient.addOneTimeListener("paused", function(event, pkt) {
    packet = pkt;
    threadClient.resume();
  });

  await threadClient.pauseOnExceptions(false, true);
  try {
    /* eslint-disable */
    await debuggee.eval(
      "(" + function() {
        function dontStopMe() {
          throw 43;
        }
        dontStopMe();
      } + ")()"
    );
    /* eslint-enable */
  } catch (e) {}

  // Test that the paused listener callback hasn't been called
  // on the thrown error from dontStopMe()
  Assert.equal(!!packet, false);

  await threadClient.pauseOnExceptions(true, true);
  try {
    /* eslint-disable */
    debuggee.eval(
      "(" + function() {
        function stopMeAgain() {
          throw 44;
        }
        stopMeAgain();
      } + ")()"
    );
    /* eslint-enable */
  } catch (e) {}

  // Test that the paused listener callback has been called
  // on the thrown error from stopMeAgain()
  Assert.equal(!!packet, true);
  Assert.equal(packet.why.type, "exception");
  Assert.equal(packet.why.exception, "44");
}, {
  // Bug 1508289, exception tests fails in worker scope
  doNotRunWorker: true,
}));
