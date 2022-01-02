// Ensure Ion inlining of Object.create(x) tests the type of x
// matches the template object.

var P1 = {};
var P2 = {};
minorgc();

function f1() {
    for (var i=0; i<100; i++) {
	var P = (i & 1) ? P1 : P2;
	var o = Object.create(P);
	assertEq(Object.getPrototypeOf(o), P);
    }
}
f1();

function f2() {
    var arr = [null, Array];
    for (var i=0; i<99; i++) {
	var p = arr[(i / 50)|0];
	var o = Object.create(p);
	assertEq(Object.getPrototypeOf(o), p);
    }
}
f2();
