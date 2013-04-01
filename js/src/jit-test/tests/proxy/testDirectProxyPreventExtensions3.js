load(libdir + "asserts.js");

// Throw a TypeError if the trap reports an extensible object as non-extensible
assertThrowsInstanceOf(function () {
    Object.preventExtensions(new Proxy({}, {
        preventExtensions: function () {
            return true;
        }
    }));
}, TypeError);
