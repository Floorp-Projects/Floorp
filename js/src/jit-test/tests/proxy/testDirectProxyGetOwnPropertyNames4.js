load(libdir + "asserts.js");

// Throw a TypeError if the trap reports the same property twice
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyNames(new Proxy({}, {
        getOwnPropertyNames: function (target) {
            return [ 'foo', 'foo' ];
        }
    }));
}, TypeError);
