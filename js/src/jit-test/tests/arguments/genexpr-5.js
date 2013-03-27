// 'arguments' binding can be closed over and outlives the function activation.

function f() {
    return (arguments for (x of [1]));
}

var args = f("ponies").next();
assertEq(args[0], "ponies");
