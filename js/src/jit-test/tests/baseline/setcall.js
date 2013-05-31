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

function test3() {
    var v1, v2, v3;
    var z = [1, 2, 3];
    for (var i=0; i<15; i++) {
       if (i > 12)
           [v1, v2, g(), v3] = z
    }
}
try {
    test3();
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof ReferenceError, true);
}
assertEq(calls, 3);
