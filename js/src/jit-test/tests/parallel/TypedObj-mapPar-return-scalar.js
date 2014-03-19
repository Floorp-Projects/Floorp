// Test basic mapPar parallel execution.

if (!this.hasOwnProperty("TypedObject"))
  quit();

load(libdir + "parallelarray-helpers.js")

var { ArrayType, StructType, uint32 } = TypedObject;

function test() {
  var L = minItemsTestingThreshold;
  var Uints = uint32.array(L);
  var uints1 = new Uints();
  assertParallelExecSucceeds(
    // FIXME Bug 983692 -- no where to pass `m` to
    function(m) uints1.mapPar(function(e) e + 1),
    function(uints2) {
      for (var i = 0; i < L; i++)
        assertEq(uints1[i] + 1, uints2[i]);
    });
}

test();

