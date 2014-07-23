load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap defines a non-configurable property that does
 * not exist on the target
 */
var handler = { defineProperty: function (target, name, desc) { return true; } };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy]) {
    assertThrowsInstanceOf(function () {
        Object.defineProperty(p, 'foo', { configurable: false });
    }, TypeError);
}
