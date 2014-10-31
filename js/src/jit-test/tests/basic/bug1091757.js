try {
    (function() {
	let a = 3;
	let XY = XY;
	return function() { return a; };
    })();
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof ReferenceError, true);
    assertEq(e.message.contains("XY"), true);
}
