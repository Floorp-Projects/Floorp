load(libdir + "asserts.js");

/*
 * Throw a TypeError if both the current descriptor and the descriptor returned
 * by the trap are data descriptors, the current descriptor is non-configurable
 * and non-writable, and the descriptor returned by the trap is writable.
 */
var target = {};
Object.defineProperty(target, 'foo', {
    value: 'bar',
    writable: false,
    configurable: false
});
assertThrowsInstanceOf(function () { 
    Object.getOwnPropertyDescriptor(new Proxy(target, {
        getOwnPropertyDescriptor: function (target, name) {
            return {
                value: 'bar',
                writable: true,
                configurable: false
            };
        }
    }), 'foo');
}, TypeError);
