/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the method defineLazyProxy from XPCOMUtils.jsm.
 */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

add_task(function test_lazy_proxy() {
  let tmp = {};
  let realObject = {
    "prop1": "value1",
    "prop2": "value2",
  };

  let evaluated = false;
  let untrapCalled = false;

  let lazyProxy = XPCOMUtils.defineLazyProxy(
    tmp,
    "myLazyProxy",

    // Initiliazer function
    function init() {
      evaluated = true;
      return realObject;
    },

    // Stub properties
    {
      "prop1": "stub"
    },

    // Untrap callback
    function untrapCallback(obj) {
      Assert.equal(obj, realObject, "The underlying object can be obtained in the untrap callback");
      untrapCalled = true;
    }
  );

  // Check that the proxy returned and the one
  // defined in tmp are the same.
  //
  // Note: Assert.strictEqual can't be used here
  // because it wants to stringify the two objects
  // compared, which defeats the lazy proxy.
  Assert.ok(lazyProxy === tmp.myLazyProxy, "Return value and object defined are the same");

  Assert.ok(Cu.isProxy(lazyProxy), "Returned value is in fact a proxy");

  // Check that just using the proxy above didn't
  // trigger the lazy getter evaluation.
  Assert.ok(!evaluated, "The lazy proxy hasn't been evaluated yet");
  Assert.ok(!untrapCalled, "The untrap callback hasn't been called yet");

  // Accessing a stubbed property returns the stub
  // value and doesn't trigger evaluation.
  Assert.equal(lazyProxy.prop1, "stub", "Accessing a stubbed property returns the stubbed value");

  Assert.ok(!evaluated, "The access to the stubbed property above didn't evaluate the lazy proxy");
  Assert.ok(!untrapCalled, "The untrap callback hasn't been called yet");

  // Now the access to another property will trigger
  // the evaluation, as expected.
  Assert.equal(lazyProxy.prop2, "value2", "Property access is correctly forwarded to the underlying object");

  Assert.ok(evaluated, "Accessing a non-stubbed property triggered the proxy evaluation");
  Assert.ok(untrapCalled, "The untrap callback was called");

  // The value of prop1 is now the real value and not the stub value.
  Assert.equal(lazyProxy.prop1, "value1", "The  value of prop1 is now the real value and not the stub one");
});

add_task(function test_module_version() {
  // Test that passing a string instead of an initialization function
  // makes this behave like a lazy module getter.
  const NET_UTIL_URI = "resource://gre/modules/NetUtil.jsm";
  let underlyingObject;

  Cu.unload(NET_UTIL_URI);

  let lazyProxy = XPCOMUtils.defineLazyProxy(
    null,
    "NetUtil",
    NET_UTIL_URI,
    null, /* no stubs */
    function untrapCallback(object) {
      underlyingObject = object;
    }
  );

  Assert.ok(!Cu.isModuleLoaded(NET_UTIL_URI), "The NetUtil module was not loaded by the lazy proxy definition");

  // Access the object, which will evaluate the proxy.
  lazyProxy.foo = "bar";

  // Module was loaded.
  Assert.ok(Cu.isModuleLoaded(NET_UTIL_URI), "The NetUtil module was loaded");

  let { NetUtil } = ChromeUtils.import(NET_UTIL_URI, {});

  // Avoids a gigantic stringification in the logs.
  Assert.ok(NetUtil === underlyingObject, "The module loaded is the same as the one directly obtained by ChromeUtils.import");

  // Proxy correctly passed the setter to the underlying object.
  Assert.equal(NetUtil.foo, "bar", "Proxy correctly passed the setter to the underlying object");

  delete lazyProxy.foo;

  // Proxy correctly passed the delete operation to the underlying object.
  Assert.ok(!NetUtil.hasOwnProperty("foo"), "Proxy correctly passed the delete operation to the underlying object");
});
