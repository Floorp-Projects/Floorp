// |jit-test| error: TypeError
load(libdir + "immutable-prototype.js");

Object.defineProperty(Object.prototype, "name",
                      { set(v) { throw new TypeError("hit name"); },
                        enumerable: true,
                        configurable: true });

function TestCase(n) {
    this.name = n;
}
new TestCase();
