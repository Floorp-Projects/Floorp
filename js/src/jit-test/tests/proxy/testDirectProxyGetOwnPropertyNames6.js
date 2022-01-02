load(libdir + "asserts.js");

// Throw a TypeError if the trap skips a non-configurable property
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});

var handler = { ownKeys: () => [] };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => Object.getOwnPropertyNames(p), TypeError);
