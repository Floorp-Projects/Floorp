load(libdir + "asserts.js");

/*
 * Throw a TypeError if the current descriptor is non-configurable and the trap
 * returns a configurable descriptor
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyDescriptor(new Proxy(target, {
        getOwnPropertyDescriptor: function (target, name) {
            return {
                configurable: true
            };
        }
    }), 'foo');
}, TypeError);
