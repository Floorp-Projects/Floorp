load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap defines a new property on a non-extensible
 * object
 */
var target = {};
Object.preventExtensions(target);

var handler = { defineProperty: function (target, name, desc) { return true; } };

for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    assertThrowsInstanceOf(function () {
        Object.defineProperty(p, 'foo', { configurable: true });
    }, TypeError);
}
