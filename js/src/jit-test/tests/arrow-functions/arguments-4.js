load(libdir + "asserts.js");

// 'arguments' is allowed in a non-arrow-function with a rest param.
assertEq((function(...rest) { return (x => arguments)(1, 2)})().length, 0);

function restAndArgs(...rest) {
    return () => eval("arguments");
}

var args = restAndArgs(1, 2, 3)();
assertEq(args.length, 3);
assertDeepEq(args[0], [1, 2, 3], "This is bogus, see bug 1175394");
assertEq(args[1], 2);
assertEq(args[2], 3);

(function() {
    return ((...rest) => {
        assertDeepEq(rest, [1, 2, 3]);
        assertEq(arguments.length, 2);
        assertEq(eval("arguments").length, 2);
    })(1, 2, 3);
})(4, 5);
