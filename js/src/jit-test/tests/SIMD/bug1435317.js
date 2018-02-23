load(libdir + 'simd.js');

var ab = new ArrayBuffer(64 * 1024);
var arr = new Uint8Array(ab);

(function(glob, imp, b) {
  "use asm";
  var arr = new glob.Uint8Array(b);
  return {}
})(this, null, ab);

function testSimdX4() {
    for (var i = 10; i --> 0;) {
        var caught;
        try {
            v = SIMD.Int32x4.load(arr, 65534);
        } catch (e) {
            caught = e;
        }
        assertEq(caught instanceof RangeError, true);
    }
}

setJitCompilerOption('ion.warmup.trigger', 0);
testSimdX4();
