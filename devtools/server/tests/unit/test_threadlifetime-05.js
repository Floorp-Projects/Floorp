/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure that releasing a pause-lifetime actorin a releaseMany returns an
 * error, but releases all the thread-lifetime actors.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gPauseGrip;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_thread_lifetime();
                           });
  });
  do_test_pending();
}

function arg_grips(frameArgs, onResponse) {
  const grips = [];
  const handler = function(response) {
    if (response.error) {
      grips.push(response.error);
    } else {
      grips.push(response.from);
    }
    if (grips.length == frameArgs.length) {
      onResponse(grips);
    }
  };
  for (let i = 0; i < frameArgs.length; i++) {
    gClient.request({ to: frameArgs[i].actor, type: "threadGrip" },
                    handler);
  }
}

function test_thread_lifetime() {
  // Get two thread-lifetime grips.
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const frameArgs = [ packet.frame.arguments[0], packet.frame.arguments[1] ];
    gPauseGrip = packet.frame.arguments[2];
    arg_grips(frameArgs, function(grips) {
      release_grips(frameArgs, grips);
    });
  });

  gDebuggee.eval("(" + function() {
    function stopMe(arg1, arg2, arg3) {
      debugger;
    }
    stopMe({obj: 1}, {obj: 2}, {obj: 3});
  } + ")()");
}

function release_grips(frameArgs, threadGrips) {
  // Release all actors with releaseMany...
  const release = [threadGrips[0], threadGrips[1], gPauseGrip.actor];
  gThreadClient.releaseMany(release, function(response) {
    Assert.equal(response.error, "notReleasable");
    // Now ask for thread grips again, they should not exist.
    arg_grips(frameArgs, function(newGrips) {
      for (let i = 0; i < newGrips.length; i++) {
        Assert.equal(newGrips[i], "noSuchActor");
      }
      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });
}
