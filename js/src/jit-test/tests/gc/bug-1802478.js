// |jit-test| skip-if: !('oomAfterAllocations' in this)

enableTrackAllocations();
for (a of "x") {
  gczeal(2, 1);
}
oomAfterAllocations(1);
try {
  newString();
} catch (x) {}
