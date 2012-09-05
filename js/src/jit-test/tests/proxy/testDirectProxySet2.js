/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, the name of the property as the second argument, the value as the
 * third argument, and the receiver as the fourth argument
 */
var target = {};
var called = false;
var handler = {
    set: function (target1, name, val, receiver) {
        assertEq(this, handler);
        assertEq(target1, target);
        assertEq(name, 'foo');
        assertEq(val, 'baz');
        assertEq(receiver, proxy);
        called = true;
    }
};
var proxy = new Proxy(target, handler);
proxy['foo'] = 'baz';
assertEq(called, true);
