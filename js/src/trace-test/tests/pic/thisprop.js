// test getthisprop

var expected = "22,22,22,;33,33,33,;";
var actual = '';

function f() {
  for (var i = 0; i < 3; ++i) {
    actual += this.b + ',';
  }
  actual += ';';
}

function A() {
  this.a = 11;
  this.b = 22;
};

A.prototype.f = f;

function B() {
  this.b = 33;
  this.c = 44;
};

B.prototype.f = f;

new A().f();
new B().f();

assertEq(actual, expected);
