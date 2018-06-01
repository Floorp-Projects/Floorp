/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_longstring_grip();
                           });
  });
  do_test_pending();
}

function test_longstring_grip() {
  const longString = "All I want is to be a monkey of moderate intelligence who"
    + " wears a suit... that's why I'm transferring to business school! Maybe I"
    + " love you so much, I love you no matter who you are pretending to be."
    + " Enough about your promiscuous mother, Hermes! We have bigger problems."
    + " For example, if you killed your grandfather, you'd cease to exist! What"
    + " kind of a father would I be if I said no? Yep, I remember. They came in"
    + " last at the Olympics, then retired to promote alcoholic beverages! And"
    + " remember, don't do anything that affects anything, unless it turns out"
    + " you were supposed to, in which case, for the love of God, don't not do"
    + " it!";

  DebuggerServer.LONG_STRING_LENGTH = 200;

  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const args = packet.frame.arguments;
    Assert.equal(args.length, 1);
    const grip = args[0];

    try {
      Assert.equal(grip.type, "longString");
      Assert.equal(grip.length, longString.length);
      Assert.equal(grip.initial,
                   longString.substr(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH));

      const longStringClient = gThreadClient.pauseLongString(grip);
      longStringClient.substring(22, 28, function(response) {
        try {
          Assert.equal(response.substring, "monkey");
        } finally {
          gThreadClient.resume(function() {
            finishClient(gClient);
          });
        }
      });
    } catch (error) {
      gThreadClient.resume(function() {
        finishClient(gClient);
        do_throw(error);
      });
    }
  });

  gDebuggee.eval('stopMe("' + longString + '")');
}

