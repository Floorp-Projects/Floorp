// Check byte counts produced by takeCensus.

let g = newGlobal();
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
    assertEq(v.bytes, sizeOfAM);
    seen++;
    break;

  case 2005:
    assertEq(v.count, 10);
    assertEq(v.bytes, 10 * sizeOfAM);
    seen++;
    break;

  default: assertEq(true, false);
  }
});

assertEq(seen, 2);
