// Test that setting Debugger.Memory.prototype.allocationSamplingProbability to
// a bad number throws.

load(libdir + "asserts.js");

const root = newGlobal();

const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

var mem = dbg.memory;

// Out of range, negative
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = -Number.MAX_VALUE,
                       TypeError);
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = -1,
                       TypeError);
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = -Number.MIN_VALUE,
                       TypeError);

// In range
mem.allocationSamplingProbability = -0.0;
mem.allocationSamplingProbability = 0.0;
mem.allocationSamplingProbability = Number.MIN_VALUE;
mem.allocationSamplingProbability = 1 / 3;
mem.allocationSamplingProbability = .5;
mem.allocationSamplingProbability = 2 / 3;
mem.allocationSamplingProbability = 1 - Math.pow(2, -53);
mem.allocationSamplingProbability = 1;

// Out of range, positive
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = 1 + Number.EPSILON,
                       TypeError);
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = 2,
                       TypeError);
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = Number.MAX_VALUE,
                       TypeError);

// Out of range, non-finite
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = -Infinity,
                       TypeError);
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = Infinity,
                       TypeError);
assertThrowsInstanceOf(() => mem.allocationSamplingProbability = NaN,
                       TypeError);
