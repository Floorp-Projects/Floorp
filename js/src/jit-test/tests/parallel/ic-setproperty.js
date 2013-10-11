load(libdir + "parallelarray-helpers.js");

function set(o, v) {
  // Padding to prevent inlining.
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  o.foo = v;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
  var foo = 0;
}
set({ foo: 0 }, 42);

function testSetPropertySlot() {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function (i) {
      var o1 = {};
      var o2 = {};
      var o3 = {};
      var o4 = {};
      // Defines .foo
      set(o1, i + 1);
      set(o2, i + 2);
      set(o3, i + 3);
      set(o4, i + 4);
      // Sets .foo
      set(o1, i + 5);
      set(o2, i + 6);
      set(o3, i + 7);
      set(o4, i + 8);
      return o1.foo + o2.foo + o3.foo + o4.foo;
    });
}

function testSetArrayLength() {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function (i) {
      var a = [];
      a.length = i;
      return a.length;
    });
}

if (getBuildConfiguration().parallelJS) {
  testSetPropertySlot();
  testSetArrayLength();
}
