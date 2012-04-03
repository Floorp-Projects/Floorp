var g;
function foo(b) {
    if (b)
        function arguments() {};
    return arguments;
}

var a = foo(true);
assertEq(typeof a, "function");
assertEq(a.name, "arguments");
