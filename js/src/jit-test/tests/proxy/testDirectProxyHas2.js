/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, and the name of the property as the second argument
 */
var target = {};
for (var key of ['foo', Symbol('bar')]) {
    var called;
    var handler = {
        has: function (target1, name) {
            assertEq(this, handler);
            assertEq(target1, target);
            assertEq(name, key);
            called = true;
        }
    };
    for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
        called = false;
        key in p;
        assertEq(called, true);
    }
}
