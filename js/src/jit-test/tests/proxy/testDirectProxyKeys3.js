load(libdir + "asserts.js");

// Throw a TypeError if the trap does not return an object
assertThrowsInstanceOf(function () {
    Object.keys(new Proxy({}, {
        keys: function (target) {
            return;
        }
    }));
}, TypeError);
