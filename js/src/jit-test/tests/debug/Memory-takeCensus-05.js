// Test that Debugger.Memory.prototype.takeCensus finds cross compartment
// wrapper GC roots.

var g = newGlobal();
var dbg = new Debugger(g);

assertEq("Int8Array" in dbg.memory.takeCensus().objects, false,
         "There shouldn't exist any typed arrays in the census.");

this.ccw = g.eval("new Int8Array()");

assertEq(dbg.memory.takeCensus().objects.Int8Array.count, 1,
         "Should have one typed array in the census, because there " +
         "is one cross-compartment wrapper.");
