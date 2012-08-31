load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a new own property on a non-extensible
 * object
 */
var target = {};
Object.preventExtensions(target);
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyDescriptor(new Proxy(target, {
        getOwnPropertyDescriptor: function (target, name) {
            return {};
        }
    }), 'foo');
}, TypeError);
