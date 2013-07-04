var count = 0;

// OSR into a finally block should not throw away the frame's
// return value.
function test1() {
    try {
	return [1, 2, 3];
    } finally {
	for (var i=0; i<20; i++) { count++; }
    }
}
assertEq(test1().toString(), "1,2,3");
assertEq(count, 20);

// OSR into the finally block, with exception pending.
function test2() {
    try {
	throw 3;
    } finally {
	for (var i=0; i<20; i++) { count++; }
    }
}
try {
    test2();
    assertEq(0, 1);
} catch(e) {
    assertEq(e, 3);
}
assertEq(count, 40);
