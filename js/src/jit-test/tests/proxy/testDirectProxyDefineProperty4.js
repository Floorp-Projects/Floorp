load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap defines a non-configurable property that does
 * not exist on the target
 */
assertThrowsInstanceOf(function () {
    Object.defineProperty(new Proxy({}, {
        defineProperty: function (target, name, desc) {
            return true;
        }
    }), 'foo', {
        configurable: false
    });
}, TypeError);
