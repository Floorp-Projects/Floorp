load(libdir + "asserts.js");

// Throw a TypeError if the object refuses to be made non-extensible
assertThrowsInstanceOf(function () {
    Object.preventExtensions(new Proxy({}, {
        preventExtensions: function () {
            return false;
        }
    }));
}, TypeError);
