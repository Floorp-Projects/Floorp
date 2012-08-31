// Return the trap result
assertEq(new Proxy(function (x, y) {
    return x + y;
}, {
    apply: function (target, receiver, args) {
        return args[0] * args[1];
    }
})(2, 3), 6);
