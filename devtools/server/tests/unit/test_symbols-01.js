/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can represent ES6 Symbols over the RDP.
 */

const URL = "foo.js";

function run_test() {
  initTestDebuggerServer();
  const debuggee = addTestGlobal("test-symbols");
  const client = new DebuggerClient(DebuggerServer.connectPipe());

  client.connect().then(function () {
    attachTestTabAndResume(client, "test-symbols", function (response, tabClient, threadClient) {
      add_task(testSymbols.bind(null, client, debuggee));
      run_next_test();
    });
  });

  do_test_pending();
}

function* testSymbols(client, debuggee) {
  const evalCode = () => {
    Components.utils.evalInSandbox(
      "(" + function () {
        var symbolWithName = Symbol("Chris");
        var symbolWithoutName = Symbol();
        var iteratorSymbol = Symbol.iterator;
        debugger;
      } + "())",
      debuggee,
      "1.8",
      URL,
      1
    );
  };

  const packet = yield executeOnNextTickAndWaitForPause(evalCode, client);
  const {
    symbolWithName,
    symbolWithoutName,
    iteratorSymbol
  } = packet.frame.environment.bindings.variables;

  equal(symbolWithName.value.type, "symbol");
  equal(symbolWithName.value.name, "Chris");

  equal(symbolWithoutName.value.type, "symbol");
  ok(!("name" in symbolWithoutName.value));

  equal(iteratorSymbol.value.type, "symbol");
  equal(iteratorSymbol.value.name, "Symbol.iterator");

  finishClient(client);
}
