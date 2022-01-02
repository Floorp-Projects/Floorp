load(libdir + 'asserts.js');

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
assertThrowsInstanceOf(test1, ReferenceError);
assertEq(calls, 1);

function test2() {
    for (var i=0; i<20; i++) {
	if (i > 18)
	    g()++;
    }
}
assertThrowsInstanceOf(test2, ReferenceError);
assertEq(calls, 2);

function test3() {
    for (var i=0; i<20; i++) {
	if (i > 18)
	    g() >>= 2;
    }
}
assertThrowsInstanceOf(test3, ReferenceError);
assertEq(calls, 3);
