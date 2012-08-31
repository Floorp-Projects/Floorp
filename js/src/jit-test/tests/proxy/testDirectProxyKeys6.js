load(libdir + "asserts.js");

// Throw a TypeError if the trap skips a non-configurable enumerable property
var target = {};
Object.defineProperty(target, 'foo', {
    enumerable: true,
    configurable: false
});
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyNames(new Proxy(target, {
        getOwnPropertyNames: function (target) {
            return [];
        }
    }));
}, TypeError);
