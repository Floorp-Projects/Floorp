/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that objects are labeled as ArrayLike only when they have sequential
// numeric keys, and if they have a length property, that it matches the number
// of numeric keys. (See Bug 1371936)

async function run_test() {
  do_test_pending();
  await run_test_with_server(DebuggerServer);
  await run_test_with_server(WorkerDebuggerServer);
  do_test_finished();
}

async function run_test_with_server(server) {
  initTestDebuggerServer(server);
  const debuggee = addTestGlobal("test-grips", server);
  debuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  const dbgClient = new DebuggerClient(server.connectPipe());
  await dbgClient.connect();
  const [,, threadClient] = await attachTestTabAndResume(dbgClient, "test-grips");

  // Currying test function so we don't have to pass the debuggee and clients
  const isArrayLike = object => test_object_grip_is_array_like(
    debuggee, dbgClient, threadClient, object);

  equal(await isArrayLike({}), false, "An empty object is not ArrayLike");
  equal(await isArrayLike({length: 0}), false,
    "An object with only a length property is not ArrayLike");
  equal(await isArrayLike({2: "two"}), false,
    "Object not starting at 0 is not ArrayLike");
  equal(await isArrayLike({0: "zero", 2: "two"}), false,
    "Object with non-consecutive numeric keys is not ArrayLike");
  equal(await isArrayLike({0: "zero", 2: "two", length: 2}), false,
    "Object with non-consecutive numeric keys is not ArrayLike");
  equal(await isArrayLike({0: "zero", 1: "one", 2: "two", three: 3}), false,
    "Object with a non-numeric property other than `length` is not ArrayLike");
  equal(await isArrayLike({0: "zero", 1: "one", 2: "two", three: 3, length: 3}), false,
    "Object with a non-numeric property other than `length` is not ArrayLike");
  equal(await isArrayLike({0: "zero", 1: "one", 2: "two", length: 30}), false,
    "Object with a wrongful `length` property is not ArrayLike");

  equal(await isArrayLike({0: "zero"}), true);
  equal(await isArrayLike({0: "zero", 1: "two"}), true);
  equal(await isArrayLike({0: "zero", 1: "one", 2: "two", length: 3}), true);

  await dbgClient.close();
}

async function test_object_grip_is_array_like(debuggee, dbgClient, threadClient, object) {
  return new Promise((resolve, reject) => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const [grip] = packet.frame.arguments;
      await threadClient.resume();
      resolve(grip.preview.kind === "ArrayLike");
    });

    debuggee.eval(`
      stopMe(${JSON.stringify(object)});
    `);
  });
}
