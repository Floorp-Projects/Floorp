/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

async function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-grips", server);
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(server.connectPipe());
  await gClient.connect();
  const [,, threadClient] = await attachTestTabAndResume(gClient, "test-grips");
  gThreadClient = threadClient;
  test_symbol_grip();
}

async function test_symbol_grip() {
  gThreadClient.addOneTimeListener("paused", async function (event, packet) {
    let [grip] = packet.frame.arguments;

    // Checks grip.preview properties.
    check_preview(grip);

    let objClient = gThreadClient.pauseGrip(grip);
    let response = await objClient.getPrototypeAndProperties();
    // Checks the result of getPrototypeAndProperties.
    check_prototype_and_properties(response);

    await gThreadClient.resume();
    await gClient.close();
    gCallback();
  });

  gDebuggee.eval(`
    stopMe({
      [Symbol()]: "first unnamed symbol",
      [Symbol()]: "second unnamed symbol",
      [Symbol("named")] : "named symbol",
      [Symbol.iterator] : function* () {
        yield 1;
        yield 2;
      },
      x: 10,
    });
  `);
}

function check_preview(grip) {
  do_check_eq(grip.class, "Object");

  const {preview} = grip;
  do_check_eq(preview.ownProperties.x.configurable, true);
  do_check_eq(preview.ownProperties.x.enumerable, true);
  do_check_eq(preview.ownProperties.x.writable, true);
  do_check_eq(preview.ownProperties.x.value, 10);

  const [
    firstUnnamedSymbol,
    secondUnnamedSymbol,
    namedSymbol,
    iteratorSymbol,
  ] = preview.ownSymbols;

  do_check_eq(firstUnnamedSymbol.name, undefined);
  do_check_eq(firstUnnamedSymbol.type, "symbol");
  do_check_eq(firstUnnamedSymbol.descriptor.configurable, true);
  do_check_eq(firstUnnamedSymbol.descriptor.enumerable, true);
  do_check_eq(firstUnnamedSymbol.descriptor.writable, true);
  do_check_eq(firstUnnamedSymbol.descriptor.value, "first unnamed symbol");

  do_check_eq(secondUnnamedSymbol.name, undefined);
  do_check_eq(secondUnnamedSymbol.type, "symbol");
  do_check_eq(secondUnnamedSymbol.descriptor.configurable, true);
  do_check_eq(secondUnnamedSymbol.descriptor.enumerable, true);
  do_check_eq(secondUnnamedSymbol.descriptor.writable, true);
  do_check_eq(secondUnnamedSymbol.descriptor.value, "second unnamed symbol");

  do_check_eq(namedSymbol.name, "named");
  do_check_eq(namedSymbol.type, "symbol");
  do_check_eq(namedSymbol.descriptor.configurable, true);
  do_check_eq(namedSymbol.descriptor.enumerable, true);
  do_check_eq(namedSymbol.descriptor.writable, true);
  do_check_eq(namedSymbol.descriptor.value, "named symbol");

  do_check_eq(iteratorSymbol.name, "Symbol.iterator");
  do_check_eq(iteratorSymbol.type, "symbol");
  do_check_eq(iteratorSymbol.descriptor.configurable, true);
  do_check_eq(iteratorSymbol.descriptor.enumerable, true);
  do_check_eq(iteratorSymbol.descriptor.writable, true);
  do_check_eq(iteratorSymbol.descriptor.value.class, "Function");
}

function check_prototype_and_properties(response) {
  do_check_eq(response.ownProperties.x.configurable, true);
  do_check_eq(response.ownProperties.x.enumerable, true);
  do_check_eq(response.ownProperties.x.writable, true);
  do_check_eq(response.ownProperties.x.value, 10);

  const [
    firstUnnamedSymbol,
    secondUnnamedSymbol,
    namedSymbol,
    iteratorSymbol,
  ] = response.ownSymbols;

  do_check_eq(firstUnnamedSymbol.name, "Symbol()");
  do_check_eq(firstUnnamedSymbol.descriptor.configurable, true);
  do_check_eq(firstUnnamedSymbol.descriptor.enumerable, true);
  do_check_eq(firstUnnamedSymbol.descriptor.writable, true);
  do_check_eq(firstUnnamedSymbol.descriptor.value, "first unnamed symbol");

  do_check_eq(secondUnnamedSymbol.name, "Symbol()");
  do_check_eq(secondUnnamedSymbol.descriptor.configurable, true);
  do_check_eq(secondUnnamedSymbol.descriptor.enumerable, true);
  do_check_eq(secondUnnamedSymbol.descriptor.writable, true);
  do_check_eq(secondUnnamedSymbol.descriptor.value, "second unnamed symbol");

  do_check_eq(namedSymbol.name, "Symbol(named)");
  do_check_eq(namedSymbol.descriptor.configurable, true);
  do_check_eq(namedSymbol.descriptor.enumerable, true);
  do_check_eq(namedSymbol.descriptor.writable, true);
  do_check_eq(namedSymbol.descriptor.value, "named symbol");

  do_check_eq(iteratorSymbol.name, "Symbol(Symbol.iterator)");
  do_check_eq(iteratorSymbol.descriptor.configurable, true);
  do_check_eq(iteratorSymbol.descriptor.enumerable, true);
  do_check_eq(iteratorSymbol.descriptor.writable, true);
  do_check_eq(iteratorSymbol.descriptor.value.class, "Function");
}

