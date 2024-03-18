// Functions shared by gc/pretenure-*.js tests

const is64bit = getBuildConfiguration("pointer-byte-size") === 8;

// Count of objects that will exceed the size of the nursery.
const nurseryCount = is64bit ? 25000 : 50000;

// Count of objects that will exceed the tenured heap collection threshold.
const tenuredCount = is64bit ? 300000 : 600000;

function setupPretenureTest() {
  // The test requires that baseline is enabled and is not bypassed with
  // --ion-eager or similar.
  let jitOptions = getJitCompilerOptions();
  if (!jitOptions['baseline.enable'] ||
      jitOptions['ion.warmup.trigger'] <= jitOptions['baseline.warmup.trigger']) {
    print("Unsupported JIT options");
    quit();
  }

  // Disable zeal modes that will interfere with this test.
  gczeal(0);

  // Restrict nursery size so we can fill it quicker, and ensure it is resized.
  let size = 1024 * 1024;
  if (gcparam("semispaceNurseryEnabled")) {
    size *= 2;
  }
  gcparam("minNurseryBytes", size);
  gcparam("maxNurseryBytes", size);

  // Limit allocation threshold so we trigger major GCs sooner.
  gcparam("allocationThreshold", 1 /* MB */);

  // Disable incremental GC so there's at most one minor GC per major GC.
  gcparam("incrementalGCEnabled", false);

  // Disable balanced heap limits to make the number of GCs predictable.
  gcparam("balancedHeapLimitsEnabled", false);

  // Force a nursery collection to apply size parameters.
  let o = {};

  gc();
}

function allocateObjects(count, longLived) {
  let array = new Array(nurseryCount);
  for (let i = 0; i < count; i++) {
    let x = {x: i};
    if (longLived) {
      array[i % nurseryCount] = x;
    } else {
      array[0] = x;
    }
  }
  return array;
}

function allocateArrays(count, longLived) {
  let array = new Array(nurseryCount);
  for (let i = 0; i < count; i++) {
    let x = [i];
    if (longLived) {
      array[i % nurseryCount] = x;
    } else {
      array[0] = x;
    }
  }
  return array;
}

function gcCounts() {
  let major = gcparam("majorGCNumber")
  let minor = gcparam("minorGCNumber");

  // Only report minor collections that didn't happen as part of a major GC.
  assertEq(minor >= major, true);
  minor -= major;

  return { minor, major };
}

function runTestAndCountCollections(thunk) {
  let initialCounts = gcCounts();
  thunk();
  let finalCounts = gcCounts();
  return { minor: finalCounts.minor - initialCounts.minor,
           major: finalCounts.major - initialCounts.major };
}
