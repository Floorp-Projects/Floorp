/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* strict mode code may not contain 'with' statements */
/* eslint-disable strict */

/**
 * Check a |with| frame actor's bindings.
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
    const env = packet.frame.environment;
    Assert.notEqual(env, undefined);

    const parentEnv = env.parent;
    Assert.notEqual(parentEnv, undefined);

    const bindings = parentEnv.bindings;
    const args = bindings.arguments;
    const vars = bindings.variables;
    Assert.equal(args.length, 1);
    Assert.equal(args[0].number.value, 10);
    Assert.equal(vars.r.value, 10);
    Assert.equal(vars.a.value, Math.PI * 100);
    Assert.equal(vars.arguments.value.class, "Arguments");
    Assert.ok(!!vars.arguments.value.actor);

    const objClient = gThreadFront.pauseGrip(env.object);
    const response = await objClient.getPrototypeAndProperties();
    Assert.equal(response.ownProperties.PI.value, Math.PI);
    Assert.equal(response.ownProperties.cos.value.type, "object");
    Assert.equal(response.ownProperties.cos.value.class, "Function");
    Assert.ok(!!response.ownProperties.cos.value.actor);

    await gThreadFront.resume();
    threadFrontTestFinished();
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(number) {
      var a;
      var r = number;
      with (Math) {
        a = PI * r * r;
        debugger;
      }
    }
    stopMe(10);
  } + ")()");
  /* eslint-enable */
}
