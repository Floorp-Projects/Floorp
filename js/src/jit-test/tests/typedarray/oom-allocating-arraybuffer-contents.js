// |jit-test| skip-if: !('oomTest' in this)

oomTest(function test() {
  // The original missing OOM check was after failure to allocate ArrayBuffer
  // contents, in the ctor call -- the particular operations after that aren't
  // important.
  new Uint8ClampedArray(256).toLocaleString('hi');
});
