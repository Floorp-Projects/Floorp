function f() {
    var args = arguments, r;

    for (var i = 0; i < args.length; i++)
        r = args[i];

    return r;
}

assertEq(f.apply(null, [1, 2, 3, 4, 5, 6]), 6)
assertEq(f.apply(null, [1, 2, 3, 4, 5]), 5)
assertEq(f.apply(null, [1, 2, 3, 4]), 4)

function g(arg) {
    var r;
    for (var i = 0; i < arg.length; i++)
        r = arg[i];
    return r;
}

assertEq(g((function () arguments).call(null, 1, 2, 3)), 3);
assertEq(g(new Float32Array(3)), 0.0);
assertEq(g([1, 2, 3, 4]), 4);
