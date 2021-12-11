load(libdir + "asserts.js");

// Throw a TypeError if the trap sets a non-writable, non-configurable property
for (var key of ['foo', Symbol.for('quux')]) {
    var target = {};
    Object.defineProperty(target, key, {
        value: 'bar',
        writable: false,
        configurable: false
    });
    var handler = { set: () => true };
    for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
        assertThrowsInstanceOf(() => p[key] = 'baz', TypeError);
}
