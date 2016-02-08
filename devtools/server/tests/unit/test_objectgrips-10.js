/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gDebuggee;
var gClient;
var gThreadClient;

// Test that closures can be inspected.

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-closures");

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-closures", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
  do_test_pending();
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let person = aPacket.frame.environment.bindings.variables.person;

    do_check_eq(person.value.class, "Object");

    let personClient = gThreadClient.pauseGrip(person.value);
    personClient.getPrototypeAndProperties(aResponse => {
      do_check_eq(aResponse.ownProperties.getName.value.class, "Function");

      do_check_eq(aResponse.ownProperties.getAge.value.class, "Function");

      do_check_eq(aResponse.ownProperties.getFoo.value.class, "Function");

      let getNameClient = gThreadClient.pauseGrip(aResponse.ownProperties.getName.value);
      let getAgeClient = gThreadClient.pauseGrip(aResponse.ownProperties.getAge.value);
      let getFooClient = gThreadClient.pauseGrip(aResponse.ownProperties.getFoo.value);
      getNameClient.getScope(aResponse => {
        do_check_eq(aResponse.scope.bindings.arguments[0].name.value, "Bob");

        getAgeClient.getScope(aResponse => {
          do_check_eq(aResponse.scope.bindings.arguments[1].age.value, 58);

          getFooClient.getScope(aResponse => {
            do_check_eq(aResponse.scope.bindings.variables.foo.value, 10);

            gThreadClient.resume(() => finishClient(gClient));
          });
        });
      });
    });

  });

  gDebuggee.eval("(" + function() {
    var PersonFactory = function(name, age) {
        var foo = 10;
        return {
          getName: function() { return name; },
          getAge: function() { return age; },
          getFoo: function() { foo = Date.now(); return foo; }
        };
    };
    var person = new PersonFactory("Bob", 58);
    debugger;
  } + ")()");
}
