// Test that Debugger.Memory.prototype.takeCensus finds GC roots that are on the
// stack.

var g = newGlobal();
var dbg = new Debugger(g);

g.eval(`
  function withTypedArrayOnStack(f) {
    (function () {
      var onStack = new Int8Array();
      f();
    }())
  }
`);

assertEq("Int8Array" in dbg.memory.takeCensus().objects, false,
         "There shouldn't exist any typed arrays in the census.");

var typedArrayCount;
g.withTypedArrayOnStack(() => {
  typedArrayCount = dbg.memory.takeCensus().objects.Int8Array.count;
});

assertEq(typedArrayCount, 1,
         "Should have one typed array in the census, because there " +
         "was one on the stack.");
