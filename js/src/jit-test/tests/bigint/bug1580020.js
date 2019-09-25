// This test was designed for asan.
//
// In the deferred mode of parsing we need to ensure that the
// asmJS parser doesn't emit the deferred big int under the
// asmJS lifo alloc.
var x = -1n;
function a() {
  'use asm';
  function f() {
    return (((((-1) >>> (0 + 0)) | 0) % 10000) >> (0 + 0)) | 0;
  }
  return f;
}
assertEq(true, a()() == x);