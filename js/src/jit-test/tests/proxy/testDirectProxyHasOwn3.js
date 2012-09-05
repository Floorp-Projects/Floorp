load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a non-configurable own property as
 * non-existent
 */
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: false
});
assertThrowsInstanceOf(function () {
    ({}).hasOwnProperty.call(Proxy(target, {
        hasOwn: function (target, name) {
            return false;
        }
    }), 'foo');
}, TypeError);
