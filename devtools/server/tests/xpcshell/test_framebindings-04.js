/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* strict mode code may not contain 'with' statements */
/* eslint-disable strict */

/**
 * Check the environment bindings of a  |with| within a |with|.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const env = await packet.frame.getEnvironment();
    Assert.notEqual(env, undefined);

    const objClient = threadFront.pauseGrip(env.object);
    let response = await objClient.getPrototypeAndProperties();
    Assert.equal(response.ownProperties.one.value, 1);
    Assert.equal(response.ownProperties.two.value, 2);
    Assert.equal(response.ownProperties.foo, undefined);

    let parentEnv = env.parent;
    Assert.notEqual(parentEnv, undefined);

    const parentClient = threadFront.pauseGrip(parentEnv.object);
    response = await parentClient.getPrototypeAndProperties();
    Assert.equal(response.ownProperties.PI.value, Math.PI);
    Assert.equal(response.ownProperties.cos.value.getGrip().type, "object");
    Assert.equal(response.ownProperties.cos.value.getGrip().class, "Function");
    Assert.ok(!!response.ownProperties.cos.value.actorID);

    parentEnv = parentEnv.parent;
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
    Assert.equal(vars.foo.value, 2 * Math.PI);

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  debuggee.eval(
    "(" +
      function() {
        function stopMe(number) {
          var a,
            obj = { one: 1, two: 2 };
          var r = number;
          with (Math) {
            a = PI * r * r;
            with (obj) {
              var foo = two * PI;
              debugger;
            }
          }
        }
        stopMe(10);
      } +
      ")()"
  );
  /* eslint-enable */
}
