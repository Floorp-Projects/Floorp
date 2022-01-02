// Adapted from a fuzz test, the origianl test was:
//
// gcparam('minNurseryBytes', 0);
// gczeal(4);
//
// And would trick the GC into a state where the nursery had been setup but
// thought that it was disabled.

load(libdir + "asserts.js");

assertErrorMessage(
  () => gcparam('minNurseryBytes', 0),
  Error,
  "Parameter value out of range");

gczeal(4);

