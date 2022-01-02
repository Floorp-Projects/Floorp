load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap does not report undefined for a non-configurable
 * accessor property that does not have a getter
 */
var target = {};
Object.defineProperty(target, 'foo', {
    set: function (value) {},
    configurable: false
});
var handler = { get: function (target, name, receiver) { return 'baz'; } };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(function () { p['foo'] }, TypeError);
