load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a new own property on a non-extensible
 * object
 */
var target = {};
Object.preventExtensions(target);

var handler = { ownKeys: () => [ 'foo' ] };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => Object.keys(p), TypeError);
