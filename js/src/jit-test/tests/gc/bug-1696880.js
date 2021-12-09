// |jit-test| skip-if: helperThreadCount() === 0

gczeal(0);
gczeal(4);
function a(b) {
  c = cacheEntry(b);
  evaluate(c, {
    saveIncrementalBytecode: true
  });
  return c;
}
offThreadDecodeStencil(a(""));
