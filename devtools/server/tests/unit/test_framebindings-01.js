/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's bindings property.
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
  gThreadFront.once("paused", async function(packet) {
    const environment = await packet.frame.getEnvironment();
    const bindings = environment.bindings;
    const args = bindings.arguments;
    const vars = bindings.variables;

    Assert.equal(args.length, 6);
    Assert.equal(args[0].number.value, 42);
    Assert.equal(args[1].bool.value, true);
    Assert.equal(args[2].string.value, "nasu");
    Assert.equal(args[3].null_.value.type, "null");
    Assert.equal(args[4].undef.value.type, "undefined");
    Assert.equal(args[5].object.value.type, "object");
    Assert.equal(args[5].object.value.class, "Object");
    Assert.ok(!!args[5].object.value.actor);

    Assert.equal(vars.a.value, 1);
    Assert.equal(vars.b.value, true);
    Assert.equal(vars.c.value.type, "object");
    Assert.equal(vars.c.value.class, "Object");
    Assert.ok(!!vars.c.value.actor);

    const objClient = gThreadFront.pauseGrip(vars.c.value);
    const response = await objClient.getPrototypeAndProperties();
    Assert.equal(response.ownProperties.a.configurable, true);
    Assert.equal(response.ownProperties.a.enumerable, true);
    Assert.equal(response.ownProperties.a.writable, true);
    Assert.equal(response.ownProperties.a.value, "a");

    Assert.equal(response.ownProperties.b.configurable, true);
    Assert.equal(response.ownProperties.b.enumerable, true);
    Assert.equal(response.ownProperties.b.writable, true);
    Assert.equal(response.ownProperties.b.value.type, "undefined");
    Assert.equal(false, "class" in response.ownProperties.b.value);

    await gThreadFront.resume();
    threadFrontTestFinished();
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(number, bool, string, null_, undef, object) {
      var a = 1;
      var b = true;
      var c = { a: "a", b: undefined };
      debugger;
    }
    stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
  } + ")()");
  /* eslint-enable */
}
