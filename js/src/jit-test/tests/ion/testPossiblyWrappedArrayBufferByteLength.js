var PossiblyWrappedArrayBufferByteLength = getSelfHostedValue("PossiblyWrappedArrayBufferByteLength");

setJitCompilerOption("ion.warmup.trigger", 50);

function testBasic() {
  var arr = [1, 2, 3];
  var tarr = new Int32Array(arr);
  var abuf = tarr.buffer;

  var arrLength = arr.length;
  var bytesPerElement = Int32Array.BYTES_PER_ELEMENT;

  var f = function() {
    assertEq(PossiblyWrappedArrayBufferByteLength(abuf), arrLength * bytesPerElement);
  };
  do {
    f();
  } while (!inIon());
}
testBasic();

function testWrapped() {
  var g = newGlobal();
g.eval(`
var arr = [1, 2, 3];
var tarr = new Int32Array(arr);
var abuf = tarr.buffer;
`);

  var abuf = g.abuf;
  var arrLength = g.arr.length;
  var bytesPerElement = g.Int32Array.BYTES_PER_ELEMENT;

  var f = function() {
    assertEq(PossiblyWrappedArrayBufferByteLength(abuf), arrLength * bytesPerElement);
  };
  do {
    f();
  } while (!inIon());
}
testWrapped();
