// Sanity test that we can accumulate matching individuals in a bucket.

var g = newGlobal();
var dbg = new Debugger(g);

var bucket = { by: "bucket" };
var count = { by: "count", count: true, bytes: false };

var all = dbg.memory.takeCensus({ breakdown: bucket });
var allCount = dbg.memory.takeCensus({ breakdown: count }).count;

var coarse = dbg.memory.takeCensus({
  breakdown: {
    by: "coarseType",
    objects: bucket,
    strings: bucket,
    scripts: bucket,
    other: bucket
  }
});
var coarseCount = dbg.memory.takeCensus({
  breakdown: {
    by: "coarseType",
    objects: count,
    strings: count,
    scripts: count,
    other: count
  }
});

assertEq(all.length > 0, true);
assertEq(all.length, allCount);

assertEq(coarse.objects.length > 0, true);
assertEq(coarseCount.objects.count, coarse.objects.length);

assertEq(coarse.strings.length > 0, true);
assertEq(coarseCount.strings.count, coarse.strings.length);

assertEq(coarse.scripts.length > 0, true);
assertEq(coarseCount.scripts.count, coarse.scripts.length);

assertEq(coarse.other.length > 0, true);
assertEq(coarseCount.other.count, coarse.other.length);

assertEq(all.length >= coarse.objects.length, true);
assertEq(all.length >= coarse.strings.length, true);
assertEq(all.length >= coarse.scripts.length, true);
assertEq(all.length >= coarse.other.length, true);

function assertIsIdentifier(id) {
  assertEq(id, Math.floor(id));
  assertEq(id > 0, true);
  assertEq(id <= Math.pow(2, 48), true);
}

all.forEach(assertIsIdentifier);
coarse.objects.forEach(assertIsIdentifier);
coarse.strings.forEach(assertIsIdentifier);
coarse.scripts.forEach(assertIsIdentifier);
coarse.other.forEach(assertIsIdentifier);
