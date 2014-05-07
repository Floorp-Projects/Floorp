(function(x) {
    for (var y = 0; y < 1; y++) {
        assertEq(Array.prototype.shift.call(arguments.callee.arguments), 0);
    }
})(0)
