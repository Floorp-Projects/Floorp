// |jit-test| exitstatus: 6
function f(x) {
    if (x === 0)
	return;
    f(x - 1);
    f(x - 1);
    f(x - 1);
    f(x - 1);
    f(x - 1);
    f(x - 1);
}
timeout(0.1);
f(100);
assertEq(0, 1);
