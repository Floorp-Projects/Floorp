load(libdir + "parallelarray-helpers.js");

function f(a) {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function(i) { return a[i]; });
}

function g(a, x) {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function(i) { return a[x]; });
}

function testICDenseElement() {
  var a1 = [];
  var a2 = [];
  var a3 = [];
  var a4 = [];
  var len = minItemsTestingThreshold;
  for (var i = 0; i < len; i++) {
    a1[i] = Math.random() * 100 | 0;
    a2[i] = Math.random() * 100 | 0;
    a3[i] = Math.random() * 100 | 0;
    a4[i] = Math.random() * 100 | 0;
  }
  f(a1); f({}); f(a2); f(a3); f(a4); f(a3); f(a1);
}

function testICTypedArrayElement() {
  var specs = [Int8Array,
               Uint8Array,
               Int16Array,
               Uint16Array,
               Int32Array,
               Uint32Array,
               Float32Array,
               Float64Array,
               Uint8ClampedArray];
  var len = minItemsTestingThreshold;
  f({});
  for (var i = 0; i < specs.length; i++) {
    var ta = new specs[i](len);
    for (var j = 0; j < len; j++)
      ta[j] = Math.random() * 100;
    f(ta);
  }
}

function testICSlotElement() {
  var o1 = { foo: 0 };
  var o2 = { foo: 0, bar: '' };
  var o3 = { foo: 0, bar: '', baz: function () { } };
  var o4 = { foo: { } };
  g(o1, "foo"); g(o2, "foo"); g(o3, "foo"); g(o2, "foo"); g(o1, "foo"); g(o4, "foo");
}

if (getBuildConfiguration().parallelJS) {
  testICDenseElement();
  testICTypedArrayElement();
  testICSlotElement();
}
