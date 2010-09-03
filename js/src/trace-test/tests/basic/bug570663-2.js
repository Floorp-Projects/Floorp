function f() {
    var x;
    for (var a = 0; a < 4; a++) {
	switch (NaN) {
	default:
	    x = a;
	}
    }
    assertEq(x, 3);
}

f();
