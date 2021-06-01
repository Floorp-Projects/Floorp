// Allocate many short-lived objects and check that we don't pretenure them.

load(libdir + "pretenure.js");

setupPretenureTest();

allocateObjects(nurseryCount, false);  // Warm up.

let { minor, major } = runTestAndCountCollections(
  () => allocateObjects(nurseryCount * 5, false)
);

// Check that after the warm up period we only do minor GCs.
print(`${minor} minor GCs, ${major} major GCs`);
assertEq(minor >= 5, true);
assertEq(major == 0, true);
