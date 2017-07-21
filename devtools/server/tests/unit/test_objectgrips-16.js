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

function test_symbol_grip() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let args = packet.frame.arguments;

    do_check_eq(args[0].class, "Object");

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototypeAndProperties(function (response) {
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

      do_check_true(response.prototype != undefined);

      let protoClient = gThreadClient.pauseGrip(response.prototype);
      protoClient.getOwnPropertyNames(function (response) {
        do_check_true(response.ownPropertyNames.toString != undefined);

        gThreadClient.resume(function () {
          gClient.close().then(gCallback);
        });
      });
    });
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

