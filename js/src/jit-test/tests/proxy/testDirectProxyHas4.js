load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports an existing own property as
 * non-existent on a non-extensible object
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: true
});
Object.preventExtensions(target);
assertThrowsInstanceOf(function () {
    'foo' in new Proxy(target, {
        has: function (target, name) {
            return false;
        }
    });
}, TypeError);
