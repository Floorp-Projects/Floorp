// Test that we don't repeatedly trigger last-ditch GCs.

// Turn of any zeal which will disrupt GC number checks.
gczeal(0);

function allocUntilFail() {
  gc(null, 'shrinking');

  const initialSize = gcparam("gcBytes");
  const initialMaxSize = gcparam("maxBytes");
  const initGCNumber = gcparam("majorGCNumber");

  // Set a small heap limit.
  gcparam("maxBytes", initialSize + 16 * 1024);

  let error;
  try {
    let a = [];
    while (true) {
      a.push(Symbol()); // Symbols are tenured.
    }
  } catch(err) {
    error = err;
  }

  const finalGCNumber = gcparam("majorGCNumber");

  // Resetore heap limit.
  gcparam("maxBytes", initialMaxSize);

  gc();
  assertEq(error, "out of memory");
  return finalGCNumber - initGCNumber;
}

// Set the time limit for skipping last ditch GCs to 5 seconds.
gcparam("minLastDitchGCPeriod", 5);
assertEq(gcparam("minLastDitchGCPeriod"), 5);

// Allocate up to the heap limit. This triggers a last ditch GC.
let gcCount = allocUntilFail();
assertEq(gcCount, 1)

// Allocate up to the limit again. The second time we fail without
// triggering a GC.
gcCount = allocUntilFail();
assertEq(gcCount, 0)

// Wait for time limit to expire.
sleep(6);

// Check we trigger a GC again.
gcCount = allocUntilFail();
assertEq(gcCount, 1)
