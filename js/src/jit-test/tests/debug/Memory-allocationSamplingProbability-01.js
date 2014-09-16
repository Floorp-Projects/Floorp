// Test that setting Debugger.Memory.prototype.allocationSamplingProbability to
// a bad number throws.

load(libdir + "asserts.js");

const root = newGlobal();

const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

// Out of range.

assertThrowsInstanceOf(() => dbg.memory.allocationSamplingProbability = -1,
                       TypeError);
assertThrowsInstanceOf(() => dbg.memory.allocationSamplingProbability = 2,
                       TypeError);

// In range
dbg.memory.allocationSamplingProbability = 0;
dbg.memory.allocationSamplingProbability = 1;
dbg.memory.allocationSamplingProbability = .5;
