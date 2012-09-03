load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap skips an existing own enumerable property on a
 * non-extensible object
 */
var target = {};
Object.defineProperty(target, 'foo', {
    enumerable: true,
    configurable: true
});
Object.preventExtensions(target);
var caught = false;
assertThrowsInstanceOf(function () {
    Object.keys(new Proxy(target, {
        keys: function (target) {
            return [];
        }
    }));
}, TypeError);
