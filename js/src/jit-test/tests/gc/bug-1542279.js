
load(libdir + "asserts.js");

assertErrorMessage(
  () => gcparam('maxNurseryBytes', 2 ** 32 - 1),
  Error,
  "Parameter value out of range");
gc()

gcparam('minNurseryBytes', 32*1024);
gcparam('maxNurseryBytes', 64*1024);
gc()

