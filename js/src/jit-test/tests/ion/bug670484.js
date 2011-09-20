// Call a function with no arguments.
function a_g() {
	return 5;
}

function a_f(g) {
	return g();
}

a_g();
assertEq(a_f(a_g), 5);

///////////////////////////////////////////////////////////////////////////////
// Call a function with one argument.
function b_g(a) {
	return a;
}

function b_f(h,b) {
	return h(5);
}
b_g(5);
assertEq(b_f(b_g,4), 5);

///////////////////////////////////////////////////////////////////////////////
// Try to confuse the register allocator.
function c_g(a) {
	return a;
}
function c_f(h,b) {
	var x = h(5);
	var y = x + 1;
	var z = h(h(y + x + 2));
	var k = 2 + z + 3;
	return h(h(h(k)));
}
c_g(2); // prime g().
assertEq(c_f(c_g,7), 18)

///////////////////////////////////////////////////////////////////////////////
// Fail during unboxing, get kicked to interpreter.
// Interpreter throws an exception; handle it.

function d_f(a) {
	return a(); // Call a known non-object. This fails in unboxing.
}
var d_x = 0;
try {
	d_f(1); // Don't assert.
} catch(e) {
	d_x = 1;
}
assertEq(d_x, 1);

///////////////////////////////////////////////////////////////////////////////
// Try passing an uncompiled function.

function e_uncompiled(a,b,c) {
	return eval("b");
}
function e_f(h) {
	return h(0,h(2,4,6),1);
}
assertEq(e_f(e_uncompiled),4);

///////////////////////////////////////////////////////////////////////////////
// Try passing a native function.

function f_app(f,n) {
	return f(n);
}
assertEq(f_app(Math.sqrt, 16), 4);

///////////////////////////////////////////////////////////////////////////////
// Handle the case where too few arguments are passed.
function g_g(a,b,c,d,e) {
	return e;
}

function g_f(g) {
	return g(2);
}

g_g();
assertEq(g_f(g_g), undefined);

///////////////////////////////////////////////////////////////////////////////
// Don't assert when given a non-function object.
function h_f(a) {
	return a();
}

var x = new Object();
var h_ret = 0;
try {
	h_f(x); // don't assert.
} catch (e) {
	h_ret = 1;
}
assertEq(h_ret, 1);

