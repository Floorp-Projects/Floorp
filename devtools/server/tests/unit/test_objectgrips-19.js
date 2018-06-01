/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gThreadClient;

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

async function run_test_with_server(server, callback) {
  initTestDebuggerServer(server);
  gDebuggee = gDebuggee = addTestGlobal("test-grips", server);
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());
  const client = new DebuggerClient(server.connectPipe());
  await client.connect();
  const [,, threadClient] = await attachTestTabAndResume(client, "test-grips");
  gThreadClient = threadClient;
  await test_wrapped_primitive_grips();
  await client.close();
  callback();
}

async function test_wrapped_primitive_grips() {
  const tests = [{
    value: true,
    class: "Boolean"
  }, {
    value: 123,
    class: "Number"
  }, {
    value: "foo",
    class: "String"
  }, {
    value: Symbol("bar"),
    class: "Symbol",
    name: "bar"
  }];
  for (const data of tests) {
    await new Promise(function(resolve) {
      gThreadClient.addOneTimeListener("paused", async function(event, packet) {
        const [grip] = packet.frame.arguments;
        check_wrapped_primitive_grip(grip, data);

        await gThreadClient.resume();
        resolve();
      });
      gDebuggee.primitive = data.value;
      gDebuggee.eval("stopMe(Object(primitive));");
    });
  }
}

function check_wrapped_primitive_grip(grip, data) {
  strictEqual(grip.class, data.class, "The grip has the proper class.");

  if (!grip.preview) {
    // In a worker thread Cu does not exist, the objects are considered unsafe and
    // can't be unwrapped, so there is no preview.
    return;
  }

  const value = grip.preview.wrappedValue;
  if (data.class === "Symbol") {
    strictEqual(value.type, "symbol", "The wrapped value grip has symbol type.");
    strictEqual(value.name, data.name, "The wrapped value grip has the proper name.");
  } else {
    strictEqual(value, data.value, "The wrapped value is the primitive one.");
  }
}
