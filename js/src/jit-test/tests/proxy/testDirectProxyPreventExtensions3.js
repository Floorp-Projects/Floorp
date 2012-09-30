load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports that the proxy has been made
 * non-extensible, while the target is still extensible
 */
assertThrowsInstanceOf(function () {
    Object.preventExtensions(new Proxy({}, {
        preventExtensions: function (target) {
            return true;
        }
    }));
}, TypeError);
