load(libdir + "asserts.js");

/* 
 * Throw a TypeError if the trap sets a non-configurable accessor property that
 * doest not have a setter
 */
var target = {};
Object.defineProperty(target, 'foo', {
    get: function () {
        return 'bar'
    },
    configurable: false
});
assertThrowsInstanceOf(function () {
    new Proxy(target, {
        set: function (target, name, val, receiver) {
            return true;
        }
    })['foo'] = 'baz';
}, TypeError);
