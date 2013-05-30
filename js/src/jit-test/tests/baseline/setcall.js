var calls = 0;
function g() {
    calls++;
};
function test1() {
    for (var i=0; i<20; i++) {
	if (i > 18)
	    g() = 2;
    }
}
try {
    test1();
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof ReferenceError, true);
}

assertEq(calls, 1);

function test2() {
    for (var i=0; i<20; i++) {
	if (i > 18)
	    g()++;
    }
}
try {
    test2();
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof ReferenceError, true);
}
assertEq(calls, 2);
