load(libdir + "parallelarray-helpers.js");

function FooStatic(i) {
  this.i = i;
}

function testCreateThisWithTemplate() {
  assertArraySeqParResultsEq(
    range(0, minItemsTestingThreshold),
    "map",
    function (i) {
      var o = new FooStatic(i);
      return o.i;
    });
}

if (getBuildConfiguration().parallelJS) {
  testCreateThisWithTemplate();
}
