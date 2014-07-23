load(libdir + "asserts.js");

// Throw a TypeError if the trap skips a non-configurable enumerable property
var target = {};
Object.defineProperty(target, 'foo', {
    enumerable: true,
    configurable: false
});

var handler = { ownKeys: () => [] };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => Object.keys(p), TypeError);
