function f(x,y) {
    eval("assertEq(arguments.callee, f)");
}
f(1,2);
