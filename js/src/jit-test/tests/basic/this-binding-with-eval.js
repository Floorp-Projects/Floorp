function Test1() {
    this.x = 8;
    this.y = 5;
    this.z = 2;
    var o = {".this": {x: 1}};
    var res;
    with (o) {
	res = this.x + (() => this.z)() + eval("this.x + (() => this.y)()");
    }
    assertEq(res, 23);
}
new Test1();

function Test2() {
    this.x = 8;
    var o = {".this": {x: 1}};
    with (o) {
	return eval("() => this.x");
    }
}
var fun = new Test2();
assertEq(fun(), 8);

function Test3() {
    this.x = 8;
    var o = {".this": {x: 1}};
    with (o) {
	assertEq(this.x, 8);
    }
}
new Test3();

function test4() {
    var o = {".this": {x: 1}};
    with (o) {
	return () => this;
    }
}
assertEq(test4()(), this);

function test5() {
    var o = {".this": {x: 1}};
    with (o) {
	return this;
    }
}
assertEq(test5(), this);

var global = this;
evaluate("with({}) { assertEq(this, global); }");
eval("with({}) { assertEq(this, global); }");
