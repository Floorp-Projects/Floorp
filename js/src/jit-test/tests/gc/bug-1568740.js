gczeal(0);
gcparam("semispaceNurseryEnabled", 0);

function setAndTest(param, value) {
  gcparam(param, value);
  assertEq(gcparam(param), value);
}

// Set a large nursery size.
setAndTest("maxNurseryBytes", 1024*1024);
setAndTest("minNurseryBytes", 1024*1024);
minorgc();
assertEq(gcparam("nurseryBytes"), 1024*1024);

// Force it to shrink by more then one half.
setAndTest("minNurseryBytes", 64*1024);
setAndTest("maxNurseryBytes", 64*1024);
minorgc();
assertEq(gcparam("nurseryBytes"), 64*1024);

