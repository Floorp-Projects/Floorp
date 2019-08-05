load(libdir + "asserts.js");

// This is out of range
assertErrorMessage(
    () => gcparam('maxNurseryBytes', 0),
    Error,
    "Parameter value out of range");

assertErrorMessage(
    () => gcparam('maxNurseryBytes', 5),
    Error,
    "Parameter value out of range");

// this is okay
gcparam('maxNurseryBytes', 1024*1024);
assertEq(gcparam('maxNurseryBytes'), 1024*1024);
gc();

