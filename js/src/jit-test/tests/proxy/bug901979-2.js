// A proxy on the prototype chain of the global should not observe anything at
// all about lazy resolution of globals.
load(libdir + "immutable-prototype.js");

var global = this;
var status = "pass";

// This is a little tricky. There are two proxies.
// 1. handler is a proxy that fails the test if you try to call a method on it.
var metaHandler = {
  get: _ => { status = "SMASH"; },
  has: _ => { status = "SMASH"; },
  invoke: _ => { status = "SMASH"; }
};
var handler = new Proxy({}, metaHandler);

// 2. Then we create a proxy using 'handler' as its handler. This means the test
//    will fail if *any* method of the handler is called, not just get/has/invoke.
var angryProxy = new Proxy(Object.create(null), handler);
if (globalPrototypeChainIsMutable()) {
  this.__proto__ = angryProxy;
  Object.prototype.__proto__ = angryProxy;
}

// Trip the alarm once, to make sure the proxies are working.
this.nonExistingProperty;
if (globalPrototypeChainIsMutable())
  assertEq(status, "SMASH");
else
  assertEq(status, "pass");

// OK. Reset the status and run the actual test.
status = "pass";
Map;
ArrayBuffer;
Date;
assertEq(status, "pass");
