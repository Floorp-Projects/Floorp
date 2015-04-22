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

  equal(typeof data.reason, "string");
  equal(typeof data.nonincrementalReason == "string" || data.nonincrementalReason === null,
        true);

  let lastStartTimestamp = 0;
  for (let i = 0; i < data.collections.length; i++) {
    let slice = data.collections[i];

    equal(slice.startTimestamp >= lastStartTimestamp, true);
    equal(slice.startTimestamp <= slice.endTimestamp, true);

    lastStartTimestamp = slice.startTimestamp;
  }

  equal(data.collections.length >= 1, true);
  slicesFound += data.collections.length;
}

function run_test() {
  do_test_pending();

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

  executeSoon(() => {
    equal(fired, true, "The GC hook should have fired at least once");

    // NUM_SLICES + 1 full gc + however many were triggered naturally (due to
    // whatever zealousness setting).
    print("Found " + slicesFound + " slices");
    equal(slicesFound >= NUM_SLICES + 1, true);

    do_test_finished();
  });
}
