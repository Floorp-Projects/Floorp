/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test out of scope objects with synchronous functions.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-object-grip");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-object-grip",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             testObjectGroup();
                           });
  });
  do_test_pending();
}

function evalCode() {
  evalCallback(gDebuggee, function runTest() {
    let ugh = [];
    let i = 0;

    (function () {
      (function () {
        ugh.push(i++);
        debugger;
      })();
    })();

    debugger;
  });
}

const testObjectGroup = Task.async(function* () {
  let packet = yield executeOnNextTickAndWaitForPause(evalCode, gClient);

  const ugh = packet.frame.environment.parent.parent.bindings.variables.ugh;
  const ughClient = yield gThreadClient.pauseGrip(ugh.value);

  packet = yield getPrototypeAndProperties(ughClient);
  packet = yield resumeAndWaitForPause(gClient, gThreadClient);

  const ugh2 = packet.frame.environment.bindings.variables.ugh;
  const ugh2Client = gThreadClient.pauseGrip(ugh2.value);

  packet = yield getPrototypeAndProperties(ugh2Client);
  Assert.equal(packet.ownProperties.length.value, 1);

  yield resume(gThreadClient);
  finishClient(gClient);
});
