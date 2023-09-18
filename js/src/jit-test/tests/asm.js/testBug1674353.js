// |jit-test| skip-if: getBuildConfiguration("pointer-byte-size") != 8

var stdlib = this;
// The significance of this constant is that it is a 31-bit value that is larger
// than the largest valid wasm32 heap size on 32-bit platforms -- valid sizes
// are limited to floor(INT32_MAX/64K).  It is however not a valid asm.js heap
// length, because too many of the low-order bits are set.
var buffer = new ArrayBuffer((2097120) * 1024);
m = (function Module(stdlib, foreign, heap) {
  "use asm";
  var mem = new stdlib.Int16Array(heap);
  return {};
})(stdlib, {}, buffer);
