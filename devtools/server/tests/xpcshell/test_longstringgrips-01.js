/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gDebuggee;
var gClient;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, client }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      gClient = client;
      test_longstring_grip();
    },
    { waitForFinish: true }
  )
);

function test_longstring_grip() {
  const longString =
    "All I want is to be a monkey of moderate intelligence who" +
    " wears a suit... that's why I'm transferring to business school! Maybe I" +
    " love you so much, I love you no matter who you are pretending to be." +
    " Enough about your promiscuous mother, Hermes! We have bigger problems." +
    " For example, if you killed your grandfather, you'd cease to exist! What" +
    " kind of a father would I be if I said no? Yep, I remember. They came in" +
    " last at the Olympics, then retired to promote alcoholic beverages! And" +
    " remember, don't do anything that affects anything, unless it turns out" +
    " you were supposed to, in which case, for the love of God, don't not do" +
    " it!";

  DevToolsServer.LONG_STRING_LENGTH = 200;

  gThreadFront.once("paused", function (packet) {
    const args = packet.frame.arguments;
    Assert.equal(args.length, 1);
    const grip = args[0];

    try {
      Assert.equal(grip.type, "longString");
      Assert.equal(grip.length, longString.length);
      Assert.equal(
        grip.initial,
        longString.substr(0, DevToolsServer.LONG_STRING_INITIAL_LENGTH)
      );

      const longStringFront = createLongStringFront(gClient, grip);
      longStringFront.substring(22, 28).then(function (response) {
        try {
          Assert.equal(response, "monkey");
        } finally {
          gThreadFront.resume().then(function () {
            finishClient(gClient);
          });
        }
      });
    } catch (error) {
      gThreadFront.resume().then(function () {
        finishClient(gClient);
        do_throw(error);
      });
    }
  });

  gDebuggee.eval(
    // These arguments are tested.
    // eslint-disable-next-line no-unused-vars
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  gDebuggee.eval('stopMe("' + longString + '")');
}
