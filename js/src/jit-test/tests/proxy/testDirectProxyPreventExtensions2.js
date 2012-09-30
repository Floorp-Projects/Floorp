/*
 * Call the trap with the handler as the this value, and the target as the first
 * argument
 */
var target = {};
var called = false;
var handler = {
    preventExtensions: function (target1) {
        assertEq(this, handler);
        assertEq(target1, target);
        called = true;
        Object.preventExtensions(target);
        return true;
    }
};
Object.preventExtensions(new Proxy(target, handler));
assertEq(called, true);
