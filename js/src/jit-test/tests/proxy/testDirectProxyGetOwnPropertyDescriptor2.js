/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, and the name of the property as the second argument
 */
var target = {};
var called = false;
var handler = {
    getOwnPropertyDescriptor: function (target1, name) {
        assertEq(this, handler);
        assertEq(target1, target);
        assertEq(name, 'foo');
        called = true;
    }
};
Object.getOwnPropertyDescriptor(new Proxy(target, handler), 'foo');
assertEq(called, true);
