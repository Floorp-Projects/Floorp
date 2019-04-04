
// This disables the nursery.
gcparam('maxNurseryBytes', 0);

assertEq(gcparam('maxNurseryBytes'), 0);
gc();



