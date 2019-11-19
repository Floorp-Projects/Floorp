/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gThreadFront;

// Test that closures can be inspected.

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_object_grip();
    },
    { waitForFinish: true }
  )
);

function test_object_grip() {
  gThreadFront.once("paused", async function(packet) {
    const environment = await packet.frame.getEnvironment();
    const person = environment.bindings.variables.person;

    Assert.equal(person.value.class, "Object");

    const personClient = gThreadFront.pauseGrip(person.value);
    let response = await personClient.getPrototypeAndProperties();
    Assert.equal(response.ownProperties.getName.value.class, "Function");

    Assert.equal(response.ownProperties.getAge.value.class, "Function");

    Assert.equal(response.ownProperties.getFoo.value.class, "Function");

    const getNameClient = gThreadFront.pauseGrip(
      response.ownProperties.getName.value
    );
    const getAgeClient = gThreadFront.pauseGrip(
      response.ownProperties.getAge.value
    );
    const getFooClient = gThreadFront.pauseGrip(
      response.ownProperties.getFoo.value
    );

    response = await getNameClient.getScope();
    let bindings = await response.scope.bindings();
    Assert.equal(bindings.arguments[0].name.value, "Bob");

    response = await getAgeClient.getScope();
    bindings = await response.scope.bindings();
    Assert.equal(bindings.arguments[1].age.value, 58);

    response = await getFooClient.getScope();
    bindings = await response.scope.bindings();
    Assert.equal(bindings.variables.foo.value, 10);

    await gThreadFront.resume();
    threadFrontTestFinished();
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    var PersonFactory = function (name, age) {
      var foo = 10;
      return {
        getName: function () { return name; },
        getAge: function () { return age; },
        getFoo: function () { foo = Date.now(); return foo; }
      };
    };
    var person = new PersonFactory("Bob", 58);
    debugger;
  } + ")()");
  /* eslint-enable */
}
