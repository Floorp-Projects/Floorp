load(libdir + "asserts.js");

function testProxy(handlerReturn, prop, shouldThrow) {
    var handler = { get: function () { return handlerReturn; } };
    for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
        if (shouldThrow)
            assertThrowsInstanceOf(function () { return p[prop]; }, TypeError);
        else
            assertEq(p[prop], handlerReturn);
    }
}

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
testProxy('baz', 'foo', true);
/*
 * Don't throw a TypeError if the trap reports the same value for a non-writable,
 * non-configurable property
 */
testProxy('bar', 'foo', false);

/*
 * Don't throw a TypeError if the trap reports a different value for a writable,
 * non-configurable property
 */
Object.defineProperty(target, 'prop', {
    value: 'bar',
    writable: true,
    configurable: false
});
testProxy('baz', 'prop', false);

/*
 * Don't throw a TypeError if the trap reports a different value for a non-writable,
 * configurable property
 */
Object.defineProperty(target, 'prop2', {
    value: 'bar',
    writable: false,
    configurable: true
});
testProxy('baz', 'prop2', false);
