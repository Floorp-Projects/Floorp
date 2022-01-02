load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap skips an existing own enumerable property on a
 * non-extensible object
 */
var target = {};
Object.defineProperty(target, 'foo', {
    enumerable: true,
    configurable: true
});
Object.preventExtensions(target);

var handler = { ownKeys: () => [] };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => Object.keys(p), TypeError);
