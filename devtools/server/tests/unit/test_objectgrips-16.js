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
  Assert.equal(grip.class, "Object");

  const {preview} = grip;
  Assert.equal(preview.ownProperties.x.configurable, true);
  Assert.equal(preview.ownProperties.x.enumerable, true);
  Assert.equal(preview.ownProperties.x.writable, true);
  Assert.equal(preview.ownProperties.x.value, 10);

  const [
    firstUnnamedSymbol,
    secondUnnamedSymbol,
    namedSymbol,
    iteratorSymbol,
  ] = preview.ownSymbols;

  Assert.equal(firstUnnamedSymbol.name, undefined);
  Assert.equal(firstUnnamedSymbol.type, "symbol");
  Assert.equal(firstUnnamedSymbol.descriptor.configurable, true);
  Assert.equal(firstUnnamedSymbol.descriptor.enumerable, true);
  Assert.equal(firstUnnamedSymbol.descriptor.writable, true);
  Assert.equal(firstUnnamedSymbol.descriptor.value, "first unnamed symbol");

  Assert.equal(secondUnnamedSymbol.name, undefined);
  Assert.equal(secondUnnamedSymbol.type, "symbol");
  Assert.equal(secondUnnamedSymbol.descriptor.configurable, true);
  Assert.equal(secondUnnamedSymbol.descriptor.enumerable, true);
  Assert.equal(secondUnnamedSymbol.descriptor.writable, true);
  Assert.equal(secondUnnamedSymbol.descriptor.value, "second unnamed symbol");

  Assert.equal(namedSymbol.name, "named");
  Assert.equal(namedSymbol.type, "symbol");
  Assert.equal(namedSymbol.descriptor.configurable, true);
  Assert.equal(namedSymbol.descriptor.enumerable, true);
  Assert.equal(namedSymbol.descriptor.writable, true);
  Assert.equal(namedSymbol.descriptor.value, "named symbol");

  Assert.equal(iteratorSymbol.name, "Symbol.iterator");
  Assert.equal(iteratorSymbol.type, "symbol");
  Assert.equal(iteratorSymbol.descriptor.configurable, true);
  Assert.equal(iteratorSymbol.descriptor.enumerable, true);
  Assert.equal(iteratorSymbol.descriptor.writable, true);
  Assert.equal(iteratorSymbol.descriptor.value.class, "Function");
}

function check_prototype_and_properties(response) {
  Assert.equal(response.ownProperties.x.configurable, true);
  Assert.equal(response.ownProperties.x.enumerable, true);
  Assert.equal(response.ownProperties.x.writable, true);
  Assert.equal(response.ownProperties.x.value, 10);

  const [
    firstUnnamedSymbol,
    secondUnnamedSymbol,
    namedSymbol,
    iteratorSymbol,
  ] = response.ownSymbols;

  Assert.equal(firstUnnamedSymbol.name, "Symbol()");
  Assert.equal(firstUnnamedSymbol.descriptor.configurable, true);
  Assert.equal(firstUnnamedSymbol.descriptor.enumerable, true);
  Assert.equal(firstUnnamedSymbol.descriptor.writable, true);
  Assert.equal(firstUnnamedSymbol.descriptor.value, "first unnamed symbol");

  Assert.equal(secondUnnamedSymbol.name, "Symbol()");
  Assert.equal(secondUnnamedSymbol.descriptor.configurable, true);
  Assert.equal(secondUnnamedSymbol.descriptor.enumerable, true);
  Assert.equal(secondUnnamedSymbol.descriptor.writable, true);
  Assert.equal(secondUnnamedSymbol.descriptor.value, "second unnamed symbol");

  Assert.equal(namedSymbol.name, "Symbol(named)");
  Assert.equal(namedSymbol.descriptor.configurable, true);
  Assert.equal(namedSymbol.descriptor.enumerable, true);
  Assert.equal(namedSymbol.descriptor.writable, true);
  Assert.equal(namedSymbol.descriptor.value, "named symbol");

  Assert.equal(iteratorSymbol.name, "Symbol(Symbol.iterator)");
  Assert.equal(iteratorSymbol.descriptor.configurable, true);
  Assert.equal(iteratorSymbol.descriptor.enumerable, true);
  Assert.equal(iteratorSymbol.descriptor.writable, true);
  Assert.equal(iteratorSymbol.descriptor.value.class, "Function");
}

