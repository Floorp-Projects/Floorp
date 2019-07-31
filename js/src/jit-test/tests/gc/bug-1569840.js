
gczeal(0);

gcparam("maxNurseryBytes", 1024*1024);
gcparam("minNurseryBytes", 1024*1024);
var obj = { foo: 'bar', baz: [1, 2, 3]};
minorgc();
assertEq(gcparam("nurseryBytes"), 1024*1024);

