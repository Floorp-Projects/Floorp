// Test that drainAllocationsLog returns some timestamps.

load(libdir + 'asserts.js');

var allocTimes = [];

allocTimes.push(timeSinceCreation());

const root = newGlobal();
const dbg = new Debugger(root);

dbg.memory.trackingAllocationSites = true;
root.eval("this.alloc1 = {}");
allocTimes.push(timeSinceCreation());
root.eval("this.alloc2 = {}");
allocTimes.push(timeSinceCreation());
root.eval("this.alloc3 = {}");
allocTimes.push(timeSinceCreation());
root.eval("this.alloc4 = {}");
allocTimes.push(timeSinceCreation());

allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length >= 4, true);
assertEq(allocs[0].timestamp >= allocTimes[0], true);
var seenAlloc = 0;
var lastIndexSeenAllocIncremented = 0;
for (i = 1; i < allocs.length; ++i) {
    assertEq(allocs[i].timestamp >= allocs[i - 1].timestamp, true);
    // It isn't possible to exactly correlate the entries in the
    // allocs array with the entries in allocTimes, because we can't
    // control exactly how many allocations are done during the course
    // of a given eval.  However, we can assume that there is some
    // allocation recorded after each entry in allocTimes.  So, we
    // track the allocTimes entry we've passed, and then after the
    // loop assert that we've seen them all.  We also assert that a
    // non-zero number of allocations has happened since the last seen
    // increment.
    while (seenAlloc < allocTimes.length
           && allocs[i].timestamp >= allocTimes[seenAlloc]) {
        assertEq(i - lastIndexSeenAllocIncremented > 0, true);
        lastIndexSeenAllocIncremented = i;
        ++seenAlloc;
    }
}
// There should be one entry left in allocTimes, because we recorded a
// time after the last possible allocation in the array.
assertEq(seenAlloc, allocTimes.length -1);
