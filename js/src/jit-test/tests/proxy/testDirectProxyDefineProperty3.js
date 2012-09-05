load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap defines a new property on a non-extensible
 * object
 */
var target = {};
Object.preventExtensions(target);
assertThrowsInstanceOf(function () {
    Object.defineProperty(new Proxy(target, {
        defineProperty: function (target, name, desc) {
            return true;
        }
    }), 'foo', {
        configurable: true
    });
}, TypeError);
