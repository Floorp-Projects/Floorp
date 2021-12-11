
load(libdir + "asserts.js");

assertErrorMessage(
  () => gcparam('minNurseryBytes', 0),
  Error,
  "Parameter value out of range");

assertErrorMessage(
  () => gcparam('maxNurseryBytes', 256*1024*1024),
  Error,
  "Parameter value out of range");

// This is both bigger than the maximum and out of range. but there's no way
// to test out of range without testing bigger than the maximum.
assertErrorMessage(
  () => gcparam('minNurseryBytes', 256*1024*1024),
  Error,
  "Parameter value out of range");

