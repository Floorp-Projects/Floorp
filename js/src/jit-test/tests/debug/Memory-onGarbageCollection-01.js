// Test basic usage of onGarbageCollection

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root)

const NUM_SLICES = root.NUM_SLICES = 10;

let fired = false;
let slicesFound = 0;

dbg.memory.onGarbageCollection = data => {
  fired = true;

  print("Got onGarbageCollection: " + JSON.stringify(data, null, 2));

  assertEq(typeof data.reason, "string");
  assertEq(typeof data.nonincrementalReason == "string" || data.nonincrementalReason === null,
           true);

  let lastStartTimestamp = 0;
  for (let i = 0; i < data.collections.length; i++) {
    let slice = data.collections[i];

    assertEq(slice.startTimestamp >= lastStartTimestamp, true);
    assertEq(slice.startTimestamp <= slice.endTimestamp, true);

    lastStartTimestamp = slice.startTimestamp;
  }

  assertEq(data.collections.length >= 1, true);
  slicesFound += data.collections.length;
}

root.eval(
  `
  this.allocs = [];

  // GC slices
  for (var i = 0; i < NUM_SLICES; i++) {
    this.allocs.push({});
    gcslice();
  }

  // Full GC
  this.allocs.push({});
  gc();
  `
);

// The hook should have been fired at least once.
assertEq(fired, true);

// NUM_SLICES + 1 full gc + however many were triggered naturally (due to
// whatever zealousness setting).
print("Found " + slicesFound + " slices");
assertEq(slicesFound >= NUM_SLICES + 1, true);
