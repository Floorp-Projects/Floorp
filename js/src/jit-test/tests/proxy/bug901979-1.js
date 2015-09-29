// A proxy on the prototype chain of the global can't intercept lazy definition of globals.
// Thanks to André Bargull for this one.
load(libdir + "immutable-prototype.js");

var global = this;
var status = "pass";
var handler = {
  get: function get(t, pk, r) { status = "FAIL get"; },
  has: function has(t, pk) { status = "FAIL has"; }
};

if (globalPrototypeChainIsMutable())
  Object.prototype.__proto__ = new Proxy(Object.create(null), handler);

Map;
assertEq(status, "pass");
