// Allocate many objects, changing the lifetime from short-lived to long-lived
// and check that we recover.

load(libdir + "pretenure.js");

setupPretenureTest();

// Phase 1: short lived.

allocateArrays(nurseryCount, false);
let { minor, major } = runTestAndCountCollections(
  () => allocateArrays(tenuredCount, false)
);

print(`${minor} minor GCs, ${major} major GCs`);
assertEq(minor >= 5, true);
assertEq(major == 0, true);

// Phase 2: long lived.

allocateArrays(tenuredCount, true);
({ minor, major } = runTestAndCountCollections(
  () => allocateArrays(tenuredCount * 5, true)
));

print(`${minor} minor GCs, ${major} major GCs`);
assertEq(minor <= 1, true);
assertEq(major >= 5, true);
