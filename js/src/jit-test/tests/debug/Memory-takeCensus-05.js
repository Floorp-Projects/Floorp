// Test that Debugger.Memory.prototype.takeCensus finds cross compartment
// wrapper GC roots.

var g = newGlobal();
var dbg = new Debugger(g);

assertEq("AllocationMarker" in dbg.memory.takeCensus().objects, false,
         "There shouldn't exist any allocation markers in the census.");

this.ccw = g.allocationMarker();

assertEq(dbg.memory.takeCensus().objects.AllocationMarker.count, 1,
         "Should have one allocation marker in the census, because there " +
         "is one cross-compartment wrapper referring to it.");
