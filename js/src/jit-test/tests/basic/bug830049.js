// |jit-test| error: TypeError
load(libdir + "immutable-prototype.js");

p = Proxy.create({
  has: function() { return function r() { return (s += ''); } },
  get: function() { throw new TypeError("hit get"); }
});

if (globalPrototypeChainIsMutable()) {
    Object.prototype.__proto__ = p;
} else {
    Object.defineProperty(Object.prototype, "name",
                          { set(v) { throw new TypeError("hit name"); },
                            enumerable: true,
                            configurable: true });
}

function TestCase(n) {
    this.name = n;
}
new TestCase();
