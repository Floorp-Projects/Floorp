// |jit-test| skip-if: !('oomAtAllocation' in this)

gczeal(10, 10);
try {
  throw 0;
} catch {
  for (let i = 1; i < 20 ; i++) {
    oomAtAllocation(i);
    try {
      newGlobal();
    } catch {}
  }
}
