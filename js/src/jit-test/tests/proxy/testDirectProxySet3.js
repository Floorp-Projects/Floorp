load(libdir + "asserts.js");

// Throw a TypeError if the trap sets a non-writable, non-configurable property
var target = {};
Object.defineProperty(target, 'foo', {
    value: 'bar',
    writable: false,
    configurable: false
});
assertThrowsInstanceOf(function () {
    new Proxy(target, {
        set: function (target, name, val, receiver) {
            return true;
        }
    })['foo'] = 'baz';
}, TypeError);
