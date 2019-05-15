/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

async function testPrincipal(options, globalPrincipal, debuggeeHasXrays) {
  const { debuggee } = options;
  let global, subsumes, isOpaque, globalIsInvisible;
  // Create a global object with the specified security principal.
  // If none is specified, use the debuggee.
  if (globalPrincipal === undefined) {
    global = debuggee;
    subsumes = true;
    isOpaque = false;
    globalIsInvisible = false;
    await test(options, { global, subsumes, isOpaque, globalIsInvisible });
    return;
  }

  const debuggeePrincipal = Cu.getObjectPrincipal(debuggee);
  const sameOrigin = debuggeePrincipal === globalPrincipal;
  subsumes = sameOrigin || debuggeePrincipal === systemPrincipal;
  for (const globalHasXrays of [true, false]) {
    isOpaque = subsumes && globalPrincipal !== systemPrincipal
                && (sameOrigin && debuggeeHasXrays || globalHasXrays);
    for (globalIsInvisible of [true, false]) {
      global = Cu.Sandbox(globalPrincipal, {
        wantXrays: globalHasXrays,
        invisibleToDebugger: globalIsInvisible,
      });
      // Previously, the Sandbox constructor would (bizarrely) waive xrays on
      // the return Sandbox if wantXrays was false. This has now been fixed,
      // but we need to mimic that behavior here to make the test continue
      // to pass.
      if (!globalHasXrays) {
        global = Cu.waiveXrays(global);
      }
      await test(options, { global, subsumes, isOpaque, globalIsInvisible });
    }
  }
}

function test({ threadClient, debuggee }, testOptions) {
  const { global } = testOptions;
  return new Promise(function(resolve) {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      // Get the grips.
      const [proxyGrip, inheritsProxyGrip, inheritsProxy2Grip] = packet.frame.arguments;

      // Check the grip of the proxy object.
      check_proxy_grip(debuggee, testOptions, proxyGrip);

      // Check the target and handler slots of the proxy object.
      const proxyClient = threadClient.pauseGrip(proxyGrip);
      const proxySlots = await proxyClient.getProxySlots();
      check_proxy_slots(debuggee, testOptions, proxyGrip, proxySlots);

      // Check the prototype and properties of the proxy object.
      const proxyResponse = await proxyClient.getPrototypeAndProperties();
      check_properties(testOptions, proxyResponse.ownProperties, true, false);
      check_prototype(debuggee, testOptions, proxyResponse.prototype, true, false);

      // Check the prototype and properties of the object which inherits from the proxy.
      const inheritsProxyClient = threadClient.pauseGrip(inheritsProxyGrip);
      const inheritsProxyResponse = await inheritsProxyClient.getPrototypeAndProperties();
      check_properties(testOptions, inheritsProxyResponse.ownProperties, false, false);
      check_prototype(debuggee, testOptions, inheritsProxyResponse.prototype, false,
        false);

      // The prototype chain was not iterated if the object was inaccessible, so now check
      // another object which inherits from the proxy, but was created in the debuggee.
      const inheritsProxy2Client = threadClient.pauseGrip(inheritsProxy2Grip);
      const inheritsProxy2Response =
        await inheritsProxy2Client.getPrototypeAndProperties();
      check_properties(testOptions, inheritsProxy2Response.ownProperties, false, true);
      check_prototype(debuggee, testOptions, inheritsProxy2Response.prototype, false,
        true);

      // Check that none of the above ran proxy traps.
      strictEqual(global.trapDidRun, false, "No proxy trap did run.");

      // Resume the debugger and finish the current test.
      await threadClient.resume();
      resolve();
    });

    // Create objects in `global`, and debug them in `debuggee`. They may get various
    // kinds of security wrappers, or no wrapper at all.
    // To detect that no proxy trap runs, the proxy handler should define all possible
    // traps, but the list is long and may change. Therefore a second proxy is used as
    // the handler, so that a single `get` trap suffices.
    global.eval(`
      var trapDidRun = false;
      var proxy = new Proxy({}, new Proxy({}, {get: (_, trap) => {
        trapDidRun = true;
        throw new Error("proxy trap '" + trap + "' was called.");
      }}));
      var inheritsProxy = Object.create(proxy, {x:{value:1}});
    `);
    const data = Cu.createObjectIn(debuggee, {defineAs: "data"});
    data.proxy = global.proxy;
    data.inheritsProxy = global.inheritsProxy;
    debuggee.eval(`
      var inheritsProxy2 = Object.create(data.proxy, {x:{value:1}});
      stopMe(data.proxy, data.inheritsProxy, inheritsProxy2);
    `);
  });
}

function check_proxy_grip(debuggee, testOptions, grip) {
  const { global, isOpaque, subsumes, globalIsInvisible } = testOptions;
  const {preview} = grip;

  if (global === debuggee) {
    // The proxy has no security wrappers.
    strictEqual(grip.class, "Proxy", "The grip has a Proxy class.");
    strictEqual(preview.ownPropertiesLength, 2, "The preview has 2 properties.");
    const props = preview.ownProperties;
    ok(props["<target>"].value, "<target> contains the [[ProxyTarget]].");
    ok(props["<handler>"].value, "<handler> contains the [[ProxyHandler]].");
  } else if (isOpaque) {
    // The proxy has opaque security wrappers.
    strictEqual(grip.class, "Opaque", "The grip has an Opaque class.");
    strictEqual(grip.ownPropertyLength, 0, "The grip has no properties.");
  } else if (!subsumes) {
    // The proxy belongs to compartment not subsumed by the debuggee.
    strictEqual(grip.class, "Restricted", "The grip has a Restricted class.");
    ok(!("ownPropertyLength" in grip), "The grip doesn't know the number of properties.");
  } else if (globalIsInvisible) {
    // The proxy belongs to an invisible-to-debugger compartment.
    strictEqual(grip.class, "InvisibleToDebugger: Object",
                "The grip has an InvisibleToDebugger class.");
    ok(!("ownPropertyLength" in grip), "The grip doesn't know the number of properties.");
  } else {
    // The proxy has non-opaque security wrappers.
    strictEqual(grip.class, "Proxy", "The grip has a Proxy class.");
    strictEqual(preview.ownPropertiesLength, 0, "The preview has no properties.");
    ok(!("<target>" in preview), "The preview has no <target> property.");
    ok(!("<handler>" in preview), "The preview has no <handler> property.");
  }
}

function check_proxy_slots(debuggee, testOptions, grip, proxySlots) {
  const { global } = testOptions;

  if (grip.class !== "Proxy") {
    strictEqual(proxySlots, undefined, "Slots can only be retrived for Proxy grips.");
  } else if (global === debuggee) {
    const { proxyTarget, proxyHandler } = proxySlots;
    strictEqual(proxyTarget.type, "object", "There is a [[ProxyTarget]] grip.");
    strictEqual(proxyHandler.type, "object", "There is a [[ProxyHandler]] grip.");
  } else {
    const { proxyTarget, proxyHandler } = proxySlots;
    strictEqual(proxyTarget.type, "undefined", "There is no [[ProxyTarget]] grip.");
    strictEqual(proxyHandler.type, "undefined", "There is no [[ProxyHandler]] grip.");
  }
}

function check_properties(testOptions, props, isProxy, createdInDebuggee) {
  const { subsumes, globalIsInvisible } = testOptions;
  const ownPropertiesLength = Reflect.ownKeys(props).length;

  if (createdInDebuggee || !isProxy && subsumes && !globalIsInvisible) {
    // The debuggee can access the properties.
    strictEqual(ownPropertiesLength, 1, "1 own property was retrieved.");
    strictEqual(props.x.value, 1, "The property has the right value.");
  } else {
    // The debuggee is not allowed to access the object.
    strictEqual(ownPropertiesLength, 0, "No own property could be retrieved.");
  }
}

function check_prototype(debuggee, testOptions, proto, isProxy, createdInDebuggee) {
  const { global, isOpaque, subsumes, globalIsInvisible } = testOptions;
  if (isOpaque && !globalIsInvisible && !createdInDebuggee) {
    // The object is or inherits from a proxy with opaque security wrappers.
    // The debuggee sees `Object.prototype` when retrieving the prototype.
    strictEqual(proto.class, "Object", "The prototype has a Object class.");
  } else if (isProxy && isOpaque && globalIsInvisible) {
    // The object is a proxy with opaque security wrappers in an invisible global.
    // The debuggee sees an inaccessible `Object.prototype` when retrieving the prototype.
    strictEqual(proto.class, "InvisibleToDebugger: Object",
                "The prototype has an InvisibleToDebugger class.");
  } else if (createdInDebuggee || !isProxy && subsumes && !globalIsInvisible) {
    // The object inherits from a proxy and has no security wrappers or non-opaque ones.
    // The debuggee sees the proxy when retrieving the prototype.
    check_proxy_grip(debuggee, { global, isOpaque, subsumes, globalIsInvisible }, proto);
  } else {
    // The debuggee is not allowed to access the object. It sees a null prototype.
    strictEqual(proto.type, "null", "The prototype is null.");
  }
}

async function run_tests_in_principal(options, debuggeePrincipal, debuggeeHasXrays) {
  const { debuggee } = options;
  debuggee.eval(function stopMe(arg1, arg2) {
    debugger;
  }.toString());

  // Test objects created in the debuggee.
  await testPrincipal(options, undefined, debuggeeHasXrays);

  // Test objects created in a system principal new global.
  await testPrincipal(options, systemPrincipal, debuggeeHasXrays);

  // Test objects created in a cross-origin null principal new global.
  await testPrincipal(options, null, debuggeeHasXrays);

  if (debuggeePrincipal === null) {
    // Test objects created in a same-origin null principal new global.
    await testPrincipal(options, Cu.getObjectPrincipal(debuggee), debuggeeHasXrays);
  }
}

// threadClientTest uses systemPrincipal by default, but let's be explicit here.
add_task(threadClientTest(options => {
  return run_tests_in_principal(options, systemPrincipal, true);
}, { principal: systemPrincipal, wantXrays: true }));
add_task(threadClientTest(options => {
  return run_tests_in_principal(options, systemPrincipal, false);
}, { principal: systemPrincipal, wantXrays: false }));

const nullPrincipal = Cc["@mozilla.org/nullprincipal;1"].createInstance(Ci.nsIPrincipal);
add_task(threadClientTest(options => {
  return run_tests_in_principal(options, nullPrincipal, true);
}, { principal: nullPrincipal, wantXrays: true }));
add_task(threadClientTest(options => {
  return run_tests_in_principal(options, nullPrincipal, false);
}, { principal: nullPrincipal, wantXrays: false }));
