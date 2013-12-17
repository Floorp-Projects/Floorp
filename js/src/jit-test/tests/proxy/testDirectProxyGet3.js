load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap reports a different value for a non-writable,
 * non-configurable property
 */
var target = {};
Object.defineProperty(target, 'foo', {
    value: 'bar',
    writable: false,
    configurable: false
});
assertThrowsInstanceOf(function () {
    new Proxy(target, {
        get: function (target, name, receiver) {
            return 'baz';
        }
    })['foo'];
}, TypeError);

/*
 * Don't throw a TypeError if the trap reports the same value for a non-writable,
 * non-configurable property
 */
assertEq(new Proxy(target, {
        get: function (target, name, receiver) {
            return 'bar';
        }
    })['foo'],
    'bar');


/*
 * Don't throw a TypeError if the trap reports a different value for a writable,
 * non-configurable property
 */
Object.defineProperty(target, 'prop', {
    value: 'bar',
    writable: true,
    configurable: false
});
assertEq(new Proxy(target, {
        get: function (target, name, receiver) {
            return 'baz';
        }
    })['prop'],
    'baz');


/*
 * Don't throw a TypeError if the trap reports a different value for a non-writable,
 * configurable property
 */
Object.defineProperty(target, 'prop2', {
    value: 'bar',
    writable: false,
    configurable: true
});
assertEq(new Proxy(target, {
        get: function (target, name, receiver) {
            return 'baz';
        }
    })['prop2'],
    'baz');
