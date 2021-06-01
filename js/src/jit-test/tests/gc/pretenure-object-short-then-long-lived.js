// Allocate many objects, changing the lifetime from short-lived to long-lived
// and check that we recover.

load(libdir + "pretenure.js");

setupPretenureTest();

// Phase 1: short lived.

allocateObjects(nurseryCount, false);
let { minor, major } = runTestAndCountCollections(
  () => allocateObjects(tenuredCount, false)
);

print(`${minor} minor GCs, ${major} major GCs`);
assertEq(minor >= 5, true);
assertEq(major == 0, true);

// Phase 2: long lived.

allocateObjects(tenuredCount, true);
({ minor, major } = runTestAndCountCollections(
  () => allocateObjects(tenuredCount * 5, true)
));

print(`${minor} minor GCs, ${major} major GCs`);
assertEq(minor <= 1, true);
assertEq(major >= 5, true);
