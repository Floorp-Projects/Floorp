/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, and the original arguments as the third argument.
 */
var target = function () {};
var handler = {
    construct: function (target1, args) {
        assertEq(this, handler);
        assertEq(target1, target);
        assertEq(args.length, 2);
        assertEq(args[0], 2);
        assertEq(args[1], 3);
    }
}
assertEq(new (new Proxy(target, handler))(2, 3), undefined);
