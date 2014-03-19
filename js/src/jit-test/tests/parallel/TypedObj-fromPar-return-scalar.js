// Test basic fromPar parallel execution using a scalar return value.

if (!getBuildConfiguration().parallelJS)
  quit();

load(libdir + "parallelarray-helpers.js")

var { uint8, uint32 } = TypedObject;

function test() {
  var L = minItemsTestingThreshold;
  var Uints = uint32.array(L);
  var Uint8s = uint8.array();

  var uint32s = new Uints();

  assertParallelExecSucceeds(
    // FIXME Bug 983692 -- no where to pass `m` to
    function(m) Uint8s.fromPar(uint32s, function(e) e + 1),
    function(uint8s) {
      for (var i = 0; i < L; i++)
        assertEq((uint32s[i] + 1) & 0xFF, uint8s[i]);
    });
}

test();

