/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gServer;
var gDebuggee;
var gClient;
var gThreadClient;
var gGlobal;
var gHasXrays;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

async function run_test_with_server(server, callback) {
  gServer = server;
  initTestDebuggerServer(server);

  // Run tests with a system principal debuggee.
  await run_tests_in_principal(systemPrincipal, "test-grips-system-principal");

  // Run tests with a null principal debuggee.
  await run_tests_in_principal(null, "test-grips-null-principal");

  callback();
}

async function run_tests_in_principal(principal, title) {
  // Prepare the debuggee.
  gDebuggee = Cu.Sandbox(principal);
  gDebuggee.__name = title;
  gServer.addTestGlobal(gDebuggee);
  gDebuggee.eval(function stopMe(arg1, arg2) {
    debugger;
  }.toString());
  gClient = new DebuggerClient(gServer.connectPipe());
  await gClient.connect();
  const [,, threadClient] = await attachTestTabAndResume(gClient, title);
  gThreadClient = threadClient;

  // Test objects created in the debuggee.
  await testPrincipal(undefined, false);

  // Test objects created in a new system principal with Xrays.
  await testPrincipal(systemPrincipal, true);

  // Test objects created in a new system principal without Xrays.
  await testPrincipal(systemPrincipal, false);

  // Test objects created in a new null principal with Xrays.
  await testPrincipal(null, true);

  // Test objects created in a new null principal without Xrays.
  await testPrincipal(null, false);

  // Finish.
  await gClient.close();
}

function testPrincipal(principal, wantXrays = true) {
  // Create a global object with the specified security principal.
  // If none is specified, use the debuggee.
  if (principal !== undefined) {
    gGlobal = Cu.Sandbox(principal, {wantXrays});
    gHasXrays = wantXrays;
  } else {
    gGlobal = gDebuggee;
    gHasXrays = false;
  }

  return new Promise(function (resolve) {
    gThreadClient.addOneTimeListener("paused", async function (event, packet) {
      // Get the grips.
      let [proxyGrip, inheritsProxyGrip] = packet.frame.arguments;

      // Check the grip of the proxy object.
      check_proxy_grip(proxyGrip);

      // Retrieve the properties of the object which inherits from a proxy,
      // and check the grip of its prototype.
      let objClient = gThreadClient.pauseGrip(inheritsProxyGrip);
      let response = await objClient.getPrototypeAndProperties();
      check_properties(response.ownProperties);
      check_prototype(response.prototype);

      // Check that none of the above ran proxy traps.
      strictEqual(gGlobal.trapDidRun, false, "No proxy trap did run.");

      // Resume the debugger and finish the current test.
      await gThreadClient.resume();
      resolve();
    });

    // Create objects in `gGlobal`, and debug them in `gDebuggee`. They may get various
    // kinds of security wrappers, or no wrapper at all.
    // To detect that no proxy trap runs, the proxy handler should define all possible
    // traps, but the list is long and may change. Therefore a second proxy is used as
    // the handler, so that a single `get` trap suffices.
    gGlobal.eval(`
      var trapDidRun = false;
      var proxy = new Proxy({}, new Proxy({}, {get: (_, trap) => {
        return function(_, arg) {
          if (trap === "has" && arg === "__exposedProps__") {
            // Tolerate this case until bug 1392026 is fixed.
            return false;
          }
          trapDidRun = true;
          throw new Error("proxy trap '" + trap + "' was called.");
        }
      }}));
      var inheritsProxy = Object.create(proxy, {x:{value:1}});
    `);
    let data = Cu.createObjectIn(gDebuggee, {defineAs: "data"});
    data.proxy = gGlobal.proxy;
    data.inheritsProxy = gGlobal.inheritsProxy;
    gDebuggee.eval("stopMe(data.proxy, data.inheritsProxy);");
  });
}

function isSystemPrincipal(obj) {
  return Cu.getObjectPrincipal(obj) === systemPrincipal;
}

function check_proxy_grip(grip) {
  const {preview} = grip;

  if (gGlobal === gDebuggee) {
    // The proxy has no security wrappers.
    strictEqual(grip.class, "Proxy", "The grip has a Proxy class.");
    ok(grip.proxyTarget, "There is a [[ProxyTarget]] grip.");
    ok(grip.proxyHandler, "There is a [[ProxyHandler]] grip.");
    strictEqual(preview.ownPropertiesLength, 2, "The preview has 2 properties.");
    let target = preview.ownProperties["<target>"].value;
    strictEqual(target, grip.proxyTarget, "<target> contains the [[ProxyTarget]].");
    let handler = preview.ownProperties["<handler>"].value;
    strictEqual(handler, grip.proxyHandler, "<handler> contains the [[ProxyHandler]].");
  } else if (!isSystemPrincipal(gDebuggee)) {
    // The debuggee is not allowed to remove the security wrappers.
    strictEqual(grip.class, "Object", "The grip has an Object class.");
    ok(!("ownPropertyLength" in grip), "The grip doesn't know the number of properties.");
  } else if (!gHasXrays || isSystemPrincipal(gGlobal)) {
    // The proxy has non-opaque security wrappers.
    strictEqual(grip.class, "Proxy", "The grip has a Proxy class.");
    ok(!("proxyTarget" in grip), "There is no [[ProxyTarget]] grip.");
    ok(!("proxyHandler" in grip), "There is no [[ProxyHandler]] grip.");
    strictEqual(preview.ownPropertiesLength, 0, "The preview has no properties.");
    ok(!("<target>" in preview), "The preview has no <target> property.");
    ok(!("<handler>" in preview), "The preview has no <handler> property.");
  } else {
    // The proxy has opaque security wrappers.
    strictEqual(grip.class, "Opaque", "The grip has an Opaque class.");
    strictEqual(grip.ownPropertyLength, 0, "The grip has no properties.");
  }
}

function check_properties(props) {
  let ownPropertiesLength = Reflect.ownKeys(props).length;

  if (!isSystemPrincipal(gDebuggee) && gDebuggee !== gGlobal) {
    // The debuggee is not allowed to access the object.
    strictEqual(ownPropertiesLength, 0, "No own property could be retrieved.");
  } else {
    // The debuggee can access the properties.
    strictEqual(ownPropertiesLength, 1, "1 own property was retrieved.");
    strictEqual(props.x.value, 1, "The property has the right value.");
  }
}

function check_prototype(proto) {
  if (!isSystemPrincipal(gDebuggee) && gDebuggee !== gGlobal) {
    // The debuggee is not allowed to access the object. It sees a null prototype.
    strictEqual(proto.type, "null", "The prototype is null.");
  } else if (!gHasXrays || isSystemPrincipal(gGlobal)) {
    // The object has no security wrappers or non-opaque ones.
    // The debuggee sees the proxy when retrieving the prototype.
    check_proxy_grip(proto);
  } else {
    // The object has opaque security wrappers.
    // The debuggee sees `Object.prototype` when retrieving the prototype.
    strictEqual(proto.class, "Object", "The prototype has a Object class.");
  }
}

