// Accessing a name that isn't a global property is a ReferenceError,
// even if a proxy is on the global's prototype chain.
load(libdir + "asserts.js");
load(libdir + "immutable-prototype.js");

var g = newGlobal();
if (globalPrototypeChainIsMutable())
  g.__proto__ = {};
assertThrowsInstanceOf(() => g.eval("s += ''"), g.ReferenceError);
