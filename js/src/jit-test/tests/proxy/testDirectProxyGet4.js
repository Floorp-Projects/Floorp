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
assertThrowsInstanceOf(function () {
    new Proxy(target, {
        get: function (target, name, receiver) {
            return 'baz';
        }
    })['foo'];
}, TypeError);
