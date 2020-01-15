/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    // Test named function
    function evalCode() {
      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );
      debuggee.eval("stopMe(stopMe)");
    }

    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evalCode(),
      threadFront
    );

    const args1 = packet1.frame.arguments;

    Assert.equal(args1[0].class, "Function");
    Assert.equal(args1[0].name, "stopMe");
    Assert.equal(args1[0].displayName, "stopMe");

    const objClient1 = threadFront.pauseGrip(args1[0]);
    const response1 = await objClient1.getParameterNames();
    Assert.equal(response1.parameterNames.length, 1);
    Assert.equal(response1.parameterNames[0], "arg1");

    await threadFront.resume();

    // Test inferred name function
    const packet2 = await executeOnNextTickAndWaitForPause(
      () =>
        debuggee.eval(
          "var o = { m: function(foo, bar, baz) { } }; stopMe(o.m)"
        ),
      threadFront
    );

    const args2 = packet2.frame.arguments;

    Assert.equal(args2[0].class, "Function");
    // No name for an anonymous function, but it should have an inferred name.
    Assert.equal(args2[0].name, undefined);
    Assert.equal(args2[0].displayName, "m");

    const objClient2 = threadFront.pauseGrip(args2[0]);
    const response2 = await objClient2.getParameterNames();
    Assert.equal(response2.parameterNames.length, 3);
    Assert.equal(response2.parameterNames[0], "foo");
    Assert.equal(response2.parameterNames[1], "bar");
    Assert.equal(response2.parameterNames[2], "baz");

    await threadFront.resume();

    // Test anonymous function
    const packet3 = await executeOnNextTickAndWaitForPause(
      () => debuggee.eval("stopMe(function(foo, bar, baz) { })"),
      threadFront
    );

    const args3 = packet3.frame.arguments;

    Assert.equal(args3[0].class, "Function");
    // No name for an anonymous function, and no inferred name, either.
    Assert.equal(args3[0].name, undefined);
    Assert.equal(args3[0].displayName, undefined);

    const objClient3 = threadFront.pauseGrip(args3[0]);
    const response3 = await objClient3.getParameterNames();
    Assert.equal(response3.parameterNames.length, 3);
    Assert.equal(response3.parameterNames[0], "foo");
    Assert.equal(response3.parameterNames[1], "bar");
    Assert.equal(response3.parameterNames[2], "baz");

    await threadFront.resume();
  })
);
