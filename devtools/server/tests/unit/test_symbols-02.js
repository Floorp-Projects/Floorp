/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't run debuggee code when getting symbol names.
 */

const URL = "foo.js";

function run_test() {
  initTestDebuggerServer();
  const debuggee = addTestGlobal("test-symbols");
  const client = new DebuggerClient(DebuggerServer.connectPipe());

  client.connect().then(function() {
    attachTestTabAndResume(client, "test-symbols",
                           function(response, targetFront, threadClient) {
                             add_task(testSymbols.bind(null, client, debuggee));
                             run_next_test();
                           });
  });

  do_test_pending();
}

async function testSymbols(client, debuggee) {
  const evalCode = () => {
    /* eslint-disable */
    Cu.evalInSandbox(
      "(" + function () {
        Symbol.prototype.toString = () => {
          throw new Error("lololol");
        };
        var sym = Symbol("le troll");
        debugger;
      } + "())",
      debuggee,
      "1.8",
      URL,
      1
    );
    /* eslint-enable */
  };

  const packet = await executeOnNextTickAndWaitForPause(evalCode, client);
  const { sym } = packet.frame.environment.bindings.variables;

  equal(sym.value.type, "symbol");
  equal(sym.value.name, "le troll");

  finishClient(client);
}
