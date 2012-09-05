load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap returns a non-configurable descriptor for a
 * non-existent property
 */
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyDescriptor(new Proxy({}, {
        getOwnPropertyDescriptor: function (target, name) {
            return {
                configurable: false
            };
        }
    }), 'foo');
}, TypeError);
