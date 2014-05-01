
function foo(a, b) {
    function bar() {
	return b;
    }
    return arguments[0] + arguments[1] + bar();
}
foo(1, 2);
