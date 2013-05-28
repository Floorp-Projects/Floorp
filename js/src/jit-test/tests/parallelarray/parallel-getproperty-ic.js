load(libdir + "parallelarray-helpers.js");

function testICProto() {
  function C() {}
  C.prototype.foo = "foo";
  var c = new C;
  compareAgainstArray(
    range(0, minItemsTestingThreshold),
    "map",
    function() { return c.foo; });
}

function f(o) {
  compareAgainstArray(
    range(0, minItemsTestingThreshold),
    "map",
    function() { return o.foo; });
}

function testICMultiple() {
  var o1 = { foo: 0 };
  var o2 = { foo: 0, bar: '' };
  var o3 = { foo: 0, bar: '', baz: function () { } };
  var o4 = { foo: { } };
  f(o1); f(o2); f(o3); f(o2); f(o1); f(o4);
}

function testICSameShapeDifferentProto() {
  function A () { this.bar = 1; }
  A.prototype.foo = "a";
  A.prototype.a = true;
  var x = new A;
  function B () { this.bar = 2; }
  B.prototype.foo = "b";
  A.prototype.b = true;
  var y = new B;

  f(x);
  f(y);
}

if (getBuildConfiguration().parallelJS) {
  testICProto();
  testICMultiple();
  testICSameShapeDifferentProto();
}
