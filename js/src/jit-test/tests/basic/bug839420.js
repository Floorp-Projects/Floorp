function f() {
    var x = undefined;
    try {
	[1, 2, 3].map(x);
	assertEq(0, 1);
    } catch(e) {
	assertEq(e.toString().includes("x is not"), true);
    }

    try {
	[1, 2, 3].filter(x, 1, 2);
	assertEq(0, 1);
    } catch(e) {
	assertEq(e.toString().includes("x is not"), true);
    }
}
f();
