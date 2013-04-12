load(libdir + "parallelarray-helpers.js");

function testIC() {
  function C() {}
  C.prototype.foo = "foo";
  var c = new C;
  assertParallelArrayModesCommute(["seq", "par"], function (m) {
    return new ParallelArray(minItemsTestingThreshold, function (i) {
      return c.foo;
    }, m);
  });
}

if (getBuildConfiguration().parallelJS)
  testIC();
