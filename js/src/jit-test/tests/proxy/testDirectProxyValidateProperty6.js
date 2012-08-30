load(libdir + "asserts.js");

/*
 * Throw a TypeError if both the current descriptor and the descriptor returned
 * by the trap are accessor descriptors, the current descriptor is
 * non-configurable, and the descriptor returned by the trap has a different
 * setter.
 */
var target = {};
Object.defineProperty(target, 'foo', {
    set: function (value) {i
        target.foo = 'bar';
    },
    configurable: false
});
var caught = false;
assertThrowsInstanceOf(function () { 
    Object.getOwnPropertyDescriptor(new Proxy(target, {
        getOwnPropertyDescriptor: function (target, name) {
            return {
                set: function (value) {
                    target.foo = 'baz';
                },
                configurable: false
            };
        }
    }), 'foo');
}, TypeError);
