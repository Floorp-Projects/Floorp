load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a non-configurable own property as
 * non-existent
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});
var handler = { has: () => false };
for (p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(function () { 'foo' in p; }, TypeError);
