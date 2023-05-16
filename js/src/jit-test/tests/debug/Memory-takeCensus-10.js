// Check counts produced by takeCensus.
//
// Note that tracking allocation sites adds unique IDs to objects which
// increases their size, making it hard to test reported sizes exactly.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

let sizeOfAM = byteSize(allocationMarker());

// Allocate a single allocation marker, and check that we can find it.
g.eval('var hold = allocationMarker();');
let census = dbg.memory.takeCensus({ breakdown: { by: 'objectClass' } });
assertEq(census.AllocationMarker.count, 1);
assertEq(census.AllocationMarker.bytes, sizeOfAM);

g.evaluate(`
           var objs = [];
           function fnerd() {
             objs.push(allocationMarker());
             for (let i = 0; i < 10; i++)
               objs.push(allocationMarker());
           }
           `,
           { fileName: 'J. Edgar Hoover', lineNumber: 2000 });

dbg.memory.allocationSamplingProbability = 1;
dbg.memory.trackingAllocationSites = true;

g.hold = null;
g.fnerd();

census = dbg.memory.takeCensus({
  breakdown: { by: 'objectClass',
               then: { by: 'allocationStack' }
             }
});

let seen = 0;
census.AllocationMarker.forEach((v, k) => {
  assertEq(k.functionDisplayName, 'fnerd');
  assertEq(k.source, 'J. Edgar Hoover');
  switch (k.line) {
  case 2003:
    assertEq(v.count, 1);
    assertEq(v.bytes >= sizeOfAM, true);
    seen++;
    break;

  case 2005:
    assertEq(v.count, 10);
    assertEq(v.bytes >= 10 * sizeOfAM, true);
    seen++;
    break;

  default: assertEq(true, false);
  }
});

assertEq(seen, 2);
