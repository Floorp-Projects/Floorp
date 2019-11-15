/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_named_function();
    },
    { waitForFinish: true }
  )
);

function test_named_function() {
  gThreadFront.once("paused", async function(packet) {
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Function");
    Assert.equal(args[0].name, "stopMe");
    Assert.equal(args[0].displayName, "stopMe");

    const objClient = gThreadFront.pauseGrip(args[0]);
    const response = await objClient.getParameterNames();
    Assert.equal(response.parameterNames.length, 1);
    Assert.equal(response.parameterNames[0], "arg1");

    await gThreadFront.resume();
    test_inferred_name_function();
  });

  gDebuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );
  gDebuggee.eval("stopMe(stopMe)");
}

function test_inferred_name_function() {
  gThreadFront.once("paused", async function(packet) {
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Function");
    // No name for an anonymous function, but it should have an inferred name.
    Assert.equal(args[0].name, undefined);
    Assert.equal(args[0].displayName, "m");

    const objClient = gThreadFront.pauseGrip(args[0]);
    const response = await objClient.getParameterNames();
    Assert.equal(response.parameterNames.length, 3);
    Assert.equal(response.parameterNames[0], "foo");
    Assert.equal(response.parameterNames[1], "bar");
    Assert.equal(response.parameterNames[2], "baz");

    await gThreadFront.resume();
    test_anonymous_function();
  });

  gDebuggee.eval("var o = { m: function(foo, bar, baz) { } }; stopMe(o.m)");
}

function test_anonymous_function() {
  gThreadFront.once("paused", async function(packet) {
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Function");
    // No name for an anonymous function, and no inferred name, either.
    Assert.equal(args[0].name, undefined);
    Assert.equal(args[0].displayName, undefined);

    const objClient = gThreadFront.pauseGrip(args[0]);
    const response = await objClient.getParameterNames();
    Assert.equal(response.parameterNames.length, 3);
    Assert.equal(response.parameterNames[0], "foo");
    Assert.equal(response.parameterNames[1], "bar");
    Assert.equal(response.parameterNames[2], "baz");

    await gThreadFront.resume();
    threadFrontTestFinished();
  });

  gDebuggee.eval("stopMe(function(foo, bar, baz) { })");
}
