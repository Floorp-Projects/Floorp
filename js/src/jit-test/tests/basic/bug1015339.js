function f(x, y) {
    for (var i=0; i<40; i++) {
	var stack = getBacktrace({args: true, locals: true, thisprops: true});
	assertEq(stack.includes("f(x = "), true);
	assertEq(stack.includes("this = "), true);
	backtrace();
    }
}
f(1, 2);
