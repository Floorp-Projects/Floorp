load(libdir + "asserts.js");

// Throw a TypeError if the trap skips a non-configurable property
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyNames(new Proxy(target, {
        ownKeys: function (target) {
            return [];
        }
    }));
}, TypeError);
