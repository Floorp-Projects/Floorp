// |jit-test| error: ReferenceError
load(libdir + "immutable-prototype.js");

p = Proxy.create({
  has: function() {},
  set: function() {}
});

if (globalPrototypeChainIsMutable())
  Object.prototype.__proto__ = p;

n = [];
(function() {
  var a = [];
  if (b) t = a.s()
})()
