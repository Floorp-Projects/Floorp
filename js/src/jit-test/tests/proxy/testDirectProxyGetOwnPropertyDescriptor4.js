load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a non-configurable property as
 * non-existent
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});
assertThrowsInstanceOf(function () {
    Object.getOwnPropertyDescriptor(new Proxy(target, {
        getOwnPropertyDescriptor: function (target, name) {
            return undefined;
        }
    }), 'foo');
}, TypeError);
