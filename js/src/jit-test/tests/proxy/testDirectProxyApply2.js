/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, the original this value as the second argument, and the original
 * arguments as the third argument.
 */
var target = function () {};
var receiver = {};
var handler = {
    apply: function (target1, receiver1, args) {
        assertEq(this, handler);
        assertEq(target1, target);
        assertEq(receiver1, receiver);
        assertEq(args.length, 2);
        assertEq(args[0], 2);
        assertEq(args[1], 3);
    }
}
new Proxy(target, handler).call(receiver, 2, 3);
