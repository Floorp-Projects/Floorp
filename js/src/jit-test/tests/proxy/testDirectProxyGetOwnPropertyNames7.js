load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap skips an existing own property on a
 * non-extensible object
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: true
});
Object.preventExtensions(target);
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyNames(new Proxy(target, {
        ownKeys: function (target) {
            return [];
        }
    }));
}, TypeError);
