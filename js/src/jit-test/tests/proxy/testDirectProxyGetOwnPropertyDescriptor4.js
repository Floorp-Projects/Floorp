load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a non-configurable property as
 * non-existent
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});

var handler = { getOwnPropertyDescriptor: () => undefined };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(p, 'foo'), TypeError);
