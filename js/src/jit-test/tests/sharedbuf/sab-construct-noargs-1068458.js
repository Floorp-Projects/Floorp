if (!this.SharedArrayBuffer)
    quit();

// Note that as of 2014-09-18 it is not correct to construct a SharedArrayBuffer without
// a length acceptable to asm.js: at-least 4K AND (power-of-two OR multiple-of-16M).
// That is likely to change however (see bug 1068684).  The test case is constructed
// to take that into account by catching exceptions.  That does not impact the
// original bug, which is an assertion in the implementation.

try {
    new SharedArrayBuffer	// No arguments
}
catch (e) {
    // Ignore it
}
