load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a new own property on a non-extensible
 * object
 */
var target = {};
Object.preventExtensions(target);
assertThrowsInstanceOf(function () {
    ({}).hasOwnProperty.call(new Proxy(target, {
        hasOwn: function (target, name) {
            return true;
        }
    }), 'foo');
}, TypeError);
