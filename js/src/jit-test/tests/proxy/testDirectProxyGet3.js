load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a different value for a non-writable,
 * non-configurable property
 */
var target = {};
Object.defineProperty(target, 'foo', {
    value: 'bar',
    writable: false,
    configurable: false
});
assertThrowsInstanceOf(function () {
    new Proxy(target, {
        get: function (target, name, receiver) {
            return 'baz';
        }
    })['foo'];
}, TypeError);
