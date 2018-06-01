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
                             test_named_function();
                           });
  });
  do_test_pending();
}

function test_named_function() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Function");
    Assert.equal(args[0].name, "stopMe");
    Assert.equal(args[0].displayName, "stopMe");

    const objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getParameterNames(function(response) {
      Assert.equal(response.parameterNames.length, 1);
      Assert.equal(response.parameterNames[0], "arg1");

      gThreadClient.resume(test_inferred_name_function);
    });
  });

  gDebuggee.eval("stopMe(stopMe)");
}

function test_inferred_name_function() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Function");
    // No name for an anonymous function, but it should have an inferred name.
    Assert.equal(args[0].name, undefined);
    Assert.equal(args[0].displayName, "m");

    const objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getParameterNames(function(response) {
      Assert.equal(response.parameterNames.length, 3);
      Assert.equal(response.parameterNames[0], "foo");
      Assert.equal(response.parameterNames[1], "bar");
      Assert.equal(response.parameterNames[2], "baz");

      gThreadClient.resume(test_anonymous_function);
    });
  });

  gDebuggee.eval("var o = { m: function(foo, bar, baz) { } }; stopMe(o.m)");
}

function test_anonymous_function() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Function");
    // No name for an anonymous function, and no inferred name, either.
    Assert.equal(args[0].name, undefined);
    Assert.equal(args[0].displayName, undefined);

    const objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getParameterNames(function(response) {
      Assert.equal(response.parameterNames.length, 3);
      Assert.equal(response.parameterNames[0], "foo");
      Assert.equal(response.parameterNames[1], "bar");
      Assert.equal(response.parameterNames[2], "baz");

      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  gDebuggee.eval("stopMe(function(foo, bar, baz) { })");
}

