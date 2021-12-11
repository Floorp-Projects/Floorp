// Check gating of shared memory features in plain js (bug 1231338).

assertEq(sharedMemoryEnabled(), !!this.SharedArrayBuffer);
assertEq(sharedMemoryEnabled(), !!this.Atomics);
