load(libdir + "asserts.js");

// Throw a TypeError if the trap sets a non-writable, non-configurable property
for (var key of ['foo', Symbol.for('quux')]) {
    var target = {};
    Object.defineProperty(target, key, {
        value: 'bar',
        writable: false,
        configurable: false
    });
    assertThrowsInstanceOf(function () {
        new Proxy(target, {
            set: function (target, name, val, receiver) {
                return true;
            }
        })[key] = 'baz';
    }, TypeError);
}
