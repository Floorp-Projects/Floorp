/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, and the name of the property as the second argument
 */
var target = {};
for (var key of ['foo', Symbol('bar')]) {
    var called = false;
    var handler = {
        has: function (target1, name) {
            assertEq(this, handler);
            assertEq(target1, target);
            assertEq(name, key);
            called = true;
        }
    };
    key in new Proxy(target, handler);
    assertEq(called, true);
}
