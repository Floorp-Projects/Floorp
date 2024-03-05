/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    // Test named function
    function evalCode() {
      debuggee.eval(
        // These arguments are tested.
        // eslint-disable-next-line no-unused-vars
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

    await threadFront.resume();
  })
);
