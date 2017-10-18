/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gServer;
var gDebuggee;
var gDebuggeeHasXrays;
var gClient;
var gThreadClient;
var gGlobal;
var gGlobalIsInvisible;
var gSubsumes;
var gIsOpaque;

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

async function run_tests_in_principal(debuggeePrincipal, title) {
  for (gDebuggeeHasXrays of [true, false]) {
    // Prepare the debuggee.
    let fullTitle = gDebuggeeHasXrays ? title + "-with-xrays" : title;
    gDebuggee = Cu.Sandbox(debuggeePrincipal, {wantXrays: gDebuggeeHasXrays});
    gDebuggee.__name = fullTitle;
    gServer.addTestGlobal(gDebuggee);
    gDebuggee.eval(function stopMe() {
      debugger;
    }.toString());
    gClient = new DebuggerClient(gServer.connectPipe());
    await gClient.connect();
    const [,, threadClient] = await attachTestTabAndResume(gClient, fullTitle);
    gThreadClient = threadClient;

    // Test objects created in the debuggee.
    await testPrincipal(undefined);

    // Test objects created in a system principal new global.
    await testPrincipal(systemPrincipal);

    // Test objects created in a cross-origin null principal new global.
    await testPrincipal(null);

    if (debuggeePrincipal === null) {
      // Test objects created in a same-origin null principal new global.
      await testPrincipal(Cu.getObjectPrincipal(gDebuggee));
    }

    // Finish.
    await gClient.close();
  }
}

async function testPrincipal(globalPrincipal) {
  // Create a global object with the specified security principal.
  // If none is specified, use the debuggee.
  if (globalPrincipal === undefined) {
    gGlobal = gDebuggee;
    gSubsumes = true;
    gIsOpaque = false;
    gGlobalIsInvisible = false;
    await test();
    return;
  }

  let debuggeePrincipal = Cu.getObjectPrincipal(gDebuggee);
  let sameOrigin = debuggeePrincipal === globalPrincipal;
  gSubsumes = sameOrigin || debuggeePrincipal === systemPrincipal;
  for (let globalHasXrays of [true, false]) {
    gIsOpaque = gSubsumes && globalPrincipal !== systemPrincipal
                && (sameOrigin && gDebuggeeHasXrays || globalHasXrays);
    for (gGlobalIsInvisible of [true, false]) {
      gGlobal = Cu.Sandbox(globalPrincipal, {
        wantXrays: globalHasXrays,
        invisibleToDebugger: gGlobalIsInvisible
      });
      await test();
    }
  }
}

function test() {
  return new Promise(function (resolve) {
    gThreadClient.addOneTimeListener("paused", async function (event, packet) {
      // Get the grips.
      let [proxyGrip, inheritsProxyGrip, inheritsProxy2Grip] = packet.frame.arguments;

      // Check the grip of the proxy object.
      check_proxy_grip(proxyGrip);

      // Check the prototype and properties of the proxy object.
      let proxyClient = gThreadClient.pauseGrip(proxyGrip);
      let proxyResponse = await proxyClient.getPrototypeAndProperties();
      check_properties(proxyResponse.ownProperties, true, false);
      check_prototype(proxyResponse.prototype, true, false);

      // Check the prototype and properties of the object which inherits from the proxy.
      let inheritsProxyClient = gThreadClient.pauseGrip(inheritsProxyGrip);
      let inheritsProxyResponse = await inheritsProxyClient.getPrototypeAndProperties();
      check_properties(inheritsProxyResponse.ownProperties, false, false);
      check_prototype(inheritsProxyResponse.prototype, false, false);

      // The prototype chain was not iterated if the object was inaccessible, so now check
      // another object which inherits from the proxy, but was created in the debuggee.
      let inheritsProxy2Client = gThreadClient.pauseGrip(inheritsProxy2Grip);
      let inheritsProxy2Response = await inheritsProxy2Client.getPrototypeAndProperties();
      check_properties(inheritsProxy2Response.ownProperties, false, true);
      check_prototype(inheritsProxy2Response.prototype, false, true);

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
        trapDidRun = true;
        throw new Error("proxy trap '" + trap + "' was called.");
      }}));
      var inheritsProxy = Object.create(proxy, {x:{value:1}});
    `);
    let data = Cu.createObjectIn(gDebuggee, {defineAs: "data"});
    data.proxy = gGlobal.proxy;
    data.inheritsProxy = gGlobal.inheritsProxy;
    gDebuggee.eval(`
      var inheritsProxy2 = Object.create(data.proxy, {x:{value:1}});
      stopMe(data.proxy, data.inheritsProxy, inheritsProxy2);
    `);
  });
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
  } else if (gIsOpaque) {
    // The proxy has opaque security wrappers.
    strictEqual(grip.class, "Opaque", "The grip has an Opaque class.");
    strictEqual(grip.ownPropertyLength, 0, "The grip has no properties.");
  } else if (!gSubsumes) {
    // The proxy belongs to compartment not subsumed by the debuggee.
    strictEqual(grip.class, "Restricted", "The grip has an Restricted class.");
    ok(!("ownPropertyLength" in grip), "The grip doesn't know the number of properties.");
  } else if (gGlobalIsInvisible) {
    // The proxy belongs to an invisible-to-debugger compartment.
    strictEqual(grip.class, "InvisibleToDebugger: Object",
                "The grip has an InvisibleToDebugger class.");
    ok(!("ownPropertyLength" in grip), "The grip doesn't know the number of properties.");
  } else {
    // The proxy has non-opaque security wrappers.
    strictEqual(grip.class, "Proxy", "The grip has a Proxy class.");
    ok(!("proxyTarget" in grip), "There is no [[ProxyTarget]] grip.");
    ok(!("proxyHandler" in grip), "There is no [[ProxyHandler]] grip.");
    strictEqual(preview.ownPropertiesLength, 0, "The preview has no properties.");
    ok(!("<target>" in preview), "The preview has no <target> property.");
    ok(!("<handler>" in preview), "The preview has no <handler> property.");
  }
}

function check_properties(props, isProxy, createdInDebuggee) {
  let ownPropertiesLength = Reflect.ownKeys(props).length;

  if (createdInDebuggee || !isProxy && gSubsumes && !gGlobalIsInvisible) {
    // The debuggee can access the properties.
    strictEqual(ownPropertiesLength, 1, "1 own property was retrieved.");
    strictEqual(props.x.value, 1, "The property has the right value.");
  } else {
    // The debuggee is not allowed to access the object.
    strictEqual(ownPropertiesLength, 0, "No own property could be retrieved.");
  }
}

function check_prototype(proto, isProxy, createdInDebuggee) {
  if (gIsOpaque && !gGlobalIsInvisible && !createdInDebuggee) {
    // The object is or inherits from a proxy with opaque security wrappers.
    // The debuggee sees `Object.prototype` when retrieving the prototype.
    strictEqual(proto.class, "Object", "The prototype has a Object class.");
  } else if (isProxy && gIsOpaque && gGlobalIsInvisible) {
    // The object is a proxy with opaque security wrappers in an invisible global.
    // The debuggee sees an inaccessible `Object.prototype` when retrieving the prototype.
    strictEqual(proto.class, "InvisibleToDebugger: Object",
                "The prototype has an InvisibleToDebugger class.");
  } else if (createdInDebuggee || !isProxy && gSubsumes && !gGlobalIsInvisible) {
    // The object inherits from a proxy and has no security wrappers or non-opaque ones.
    // The debuggee sees the proxy when retrieving the prototype.
    check_proxy_grip(proto);
  } else {
    // The debuggee is not allowed to access the object. It sees a null prototype.
    strictEqual(proto.type, "null", "The prototype is null.");
  }
}
