
// Properties cleared in the middle of a single function constructor.

function foo(x, y) {
  this.f = 0;
  this.g = x + y;
  this.h = 2;
}

var called = false;
var a = 0;
var b = {valueOf: function() { Object.defineProperty(Object.prototype, 'g', {set: function() { called = true }}) }};
var c = new foo(a, b);

assertEq(called, true);
assertEq(c.g, undefined);

// Properties cleared in the middle of a constructor callee.

function foo2(x, y) {
  this.a = 0;
  this.b = 1;
  bar2.call(this, x, y);
  this.c = 2;
}
function bar2(x, y) {
  this.d = x + y;
  this.e = 3;
}

var called2 = false;
var xa = 0;
var xb = {valueOf: function() { Object.defineProperty(Object.prototype, 'e', {set: function() { called2 = true }}) }};
var xc = new foo2(xa, xb);

assertEq(called2, true);
assertEq(xc.e, undefined);
assertEq(xc.c, 2);

// Properties cleared after a constructor callee.

function foo3() {
  this.aa = 0;
  this.bb = 1;
  bar3.call(this);
  this.cc = 2;
  baz();
  xbar3.call(this);
  this.dd = 3;
}
function bar3() {
  this.ee = 4;
}
function xbar3() {
  this.ff = 5;
}
function baz() {
  eval("xbar3.call = function() { called3 = true }");
}

var called3 = false;
var c3 = new foo3();
assertEq(c3.cc, 2);
assertEq(c3.ee, 4);
assertEq(c3.ff, undefined);
assertEq(c3.dd, 3);
assertEq(called3, true);
