load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap sets a non-configurable accessor property that
 * doest not have a setter
 */
var target = {};
Object.defineProperty(target, 'foo', {
    get: function () {
        return 'bar'
    },
    configurable: false
});

var handler = { set: () => true };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => p['foo'] = 'baz', TypeError);
