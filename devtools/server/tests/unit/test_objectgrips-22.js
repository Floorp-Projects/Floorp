/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gThreadClient;

add_task(async function run_test() {
  await run_test_with_server(DebuggerServer);
  await run_test_with_server(WorkerDebuggerServer);
});

async function run_test_with_server(server) {
  initTestDebuggerServer(server);
  let title = "test_enum_symbols";
  gDebuggee = addTestGlobal(title, server);
  gDebuggee.eval(function stopMe(arg) {
    debugger;
  }.toString());
  let client = new DebuggerClient(server.connectPipe());
  await client.connect();
  [,, gThreadClient] = await attachTestTabAndResume(client, title);
  await test_enum_symbols();
  await client.close();
}

async function test_enum_symbols() {
  await new Promise(function(resolve) {
    gThreadClient.addOneTimeListener("paused", async function(event, packet) {
      let [grip] = packet.frame.arguments;
      let objClient = gThreadClient.pauseGrip(grip);
      let {iterator} = await objClient.enumSymbols();
      let {ownSymbols} = await iterator.slice(0, iterator.count);

      strictEqual(ownSymbols.length, 1, "There is 1 symbol property.");
      let {name, descriptor} = ownSymbols[0];
      strictEqual(name, "Symbol(sym)", "Got right symbol name.");
      deepEqual(descriptor, {
        configurable: false,
        enumerable: false,
        writable: false,
        value: 1,
      }, "Got right property descriptor.");

      await gThreadClient.resume();
      resolve();
    });
    gDebuggee.eval(`stopMe(Object.defineProperty({}, Symbol("sym"), {value: 1}));`);
  });
}
