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

function addTestNullPrincipalGlobal(name, server = DebuggerServer) {
  // System principal objects are considered safe even when not wrapped in Xray,
  // and therefore proxy traps may run. So the test needs to use a null principal.
  let global = Cu.Sandbox(null);
  global.__name = name;
  server.addTestGlobal(global);
  return global;
}

async function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestNullPrincipalGlobal("test-grips", server);
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
    let [proxyGrip, inheritsProxyGrip] = packet.frame.arguments;

    // Check the grip of the proxy object.
    check_proxy_grip(proxyGrip);

    // Retrieve the properties of the object which inherits from a proxy,
    // and check the grip of its prototype.
    let objClient = gThreadClient.pauseGrip(inheritsProxyGrip);
    let response = await objClient.getPrototypeAndProperties();
    check_prototype_and_properties(response);

    // Check that none of the above ran proxy traps.
    let trapDidRun = gDebuggee.eval("trapDidRun");
    strictEqual(trapDidRun, false, "No proxy trap did run.");

    await gThreadClient.resume();
    await gClient.close();
    gCallback();
  });

  gDebuggee.eval(`{
    var trapDidRun = false;
    var proxy = new Proxy({}, new Proxy({}, {get: (_, trap) => {
      trapDidRun = true;
      throw new Error("proxy " + trap + " trap was called.");
    }}));
    var inheritsProxy = Object.create(proxy, {x:{value:1}});
    stopMe(proxy, inheritsProxy);
  }`);
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

function check_prototype_and_properties(response) {
  let ownPropertiesLength = Reflect.ownKeys(response.ownProperties).length;
  strictEqual(ownPropertiesLength, 1, "1 own property was retrieved.");
  strictEqual(response.ownProperties.x.value, 1, "The property has the right value.");
  check_proxy_grip(response.prototype);
}

