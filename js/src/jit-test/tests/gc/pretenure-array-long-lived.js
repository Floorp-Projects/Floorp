// Allocate many long-lived objects and check that we pretenure them.

load(libdir + "pretenure.js");

setupPretenureTest();

allocateArrays(nurseryCount, true);  // Warm up.

let { minor, major } = runTestAndCountCollections(
  () => allocateArrays(tenuredCount, true)
);

// Check that after the warm up period we 'only' do major GCs (we allow one
// nursery collection).
print(`${minor} minor GCs, ${major} major GCs`);
assertEq(minor <= 1, true);
assertEq(major >= 5, true);
