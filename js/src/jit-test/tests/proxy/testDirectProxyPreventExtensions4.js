load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports that the proxy cannot be made
 * non-extensible
 */
assertThrowsInstanceOf(function () {
    Object.preventExtensions(new Proxy({}, {
        preventExtensions: function (target) {
            return false;
        }
    }));
}, TypeError);
