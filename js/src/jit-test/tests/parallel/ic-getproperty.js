load(libdir + "parallelarray-helpers.js");

function testICProto() {
  function C() {}
  C.prototype.foo = "foo";
  var c = new C;
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function() { return c.foo; });
}

function f(o) {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function() { return o.foo; });
}

function g(o) {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function() { return o.length; });
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

function testICArrayLength() {
  var o1 = { length: 42 };
  var o2 = [1,2,3,4];
  g(o1); g(o2);
}

function testICTypedArrayLength() {
  var o1 = { length: 42 };
  var o2 = new Int8Array(128);
  var o3 = new Uint8Array(128);
  var o4 = new Uint8ClampedArray(128);
  var o5 = new Int16Array(128);
  var o6 = new Uint16Array(128);
  var o7 = new Int32Array(128);
  var o8 = new Uint32Array(128);
  var o9 = new Float32Array(128);
  var o0 = new Float64Array(128);
  g(o1); g(o2); g(o3); g(o4); g(o5); g(o6); g(o7); g(o8); g(o9); g(o0);
}

if (getBuildConfiguration().parallelJS) {
  testICProto();
  testICMultiple();
  testICSameShapeDifferentProto();
  testICArrayLength();
  testICTypedArrayLength();
}
