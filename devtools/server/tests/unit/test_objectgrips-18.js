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
  gDebuggee.eval(function stopMe(arg1, arg2) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(server.connectPipe());
  await gClient.connect();
  const [,, threadClient] = await attachTestTabAndResume(gClient, "test-grips");
  gThreadClient = threadClient;
  test_proxy_grip();
}

async function test_proxy_grip() {
  gThreadClient.addOneTimeListener("paused", async function (event, packet) {
    let [opaqueProxyGrip, transparentProxyGrip] = packet.frame.arguments;

    // Check the grip of the proxy in an opaque wrapper.
    check_opaque_grip(opaqueProxyGrip);

    // Check the grip of the proxy in a transparent wrapper.
    check_proxy_grip(transparentProxyGrip);

    // Check that none of the above ran proxy traps.
    let trapDidRun = gDebuggee.eval("global.eval('trapDidRun')");
    strictEqual(trapDidRun, false, "No proxy trap did run.");

    await gThreadClient.resume();
    await gClient.close();
    gCallback();
  });

  // Create a proxy in a null principal, and debug it in a system principal context.
  // The proxy will have an opaque wrapper, or a transparent one when waiving Xrays.
  // To detect that no proxy trap runs, the proxy handler should define all possible
  // traps, but the list is long and may change. Therefore a second proxy is used as
  // the handler, so that a single `get` trap suffices.
  gDebuggee.eval(`
    var global = Components.utils.Sandbox(null);
    var proxy = global.eval(\`
      var trapDidRun = false;
      new Proxy({}, new Proxy({}, {get: (_, trap) => {
        trapDidRun = true;
        throw new Error("proxy " + trap + " trap was called.");
      }}));
    \`);
    stopMe(proxy, Components.utils.waiveXrays(proxy));
  `);
}

function check_opaque_grip(grip) {
  strictEqual(grip.class, "Opaque", "The grip has an Opaque class.");
  strictEqual(grip.ownPropertyLength, 0, "The grip has no properties.");
}

function check_proxy_grip(grip) {
  strictEqual(grip.class, "Proxy", "The grip has a Proxy class.");
  ok(grip.proxyTarget, "There is a [[ProxyTarget]] grip.");
  ok(grip.proxyHandler, "There is a [[ProxyHandler]] grip.");

  const {preview} = grip;
  strictEqual(preview.ownPropertiesLength, 2, "The preview has 2 properties.");
  let target = preview.ownProperties["<target>"].value;
  strictEqual(target, grip.proxyTarget, "<target> contains the [[ProxyTarget]].");
  let handler = preview.ownProperties["<handler>"].value;
  strictEqual(handler, grip.proxyHandler, "<handler> contains the [[ProxyHandler]].");
}
