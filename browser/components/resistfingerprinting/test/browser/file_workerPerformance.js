function ok(a, msg) {
  postMessage({type: "status", status: !!a, msg});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({type: "finish"});
}

let isRounded = (x, expectedPrecision) => {
  let rounded = (Math.floor(x / expectedPrecision) * expectedPrecision);
  // First we do the perfectly normal check that should work just fine
  if (rounded === x || x === 0)
    return true;

  // When we're diving by non-whole numbers, we may not get perfect
  // multiplication/division because of floating points
  if (Math.abs(rounded - x + expectedPrecision) < .0000001) {
    return true;
  } else if (Math.abs(rounded - x) < .0000001) {
    return true;
  }

  // Then we handle the case where you're sub-millisecond and the timer is not
  // We check that the timer is not sub-millisecond by assuming it is not if it
  // returns an even number of milliseconds
  if (expectedPrecision < 1 && Math.round(x) == x) {
    if (Math.round(rounded) == x) {
      return true;
    }
  }

  ok(false, "Looming Test Failure, Additional Debugging Info: Expected Precision: " + expectedPrecision + " Measured Value: " + x +
    " Rounded Vaue: " + rounded + " Fuzzy1: " + Math.abs(rounded - x + expectedPrecision) +
    " Fuzzy 2: " + Math.abs(rounded - x));

  return false;
};

function runRPTests(expectedPrecision) {
  ok(isRounded(performance.timeOrigin, expectedPrecision), `In a worker, for resistFingerprinting, performance.timeOrigin is not correctly rounded: ` + performance.timeOrigin);

  // Try to add some entries.
  performance.mark("Test");
  performance.mark("Test-End");
  performance.measure("Test-Measure", "Test", "Test-End");

  // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
  is(performance.getEntries().length, 0, "In a worker, for resistFingerprinting: No entries for performance.getEntries() for workers");
  is(performance.getEntriesByType("resource").length, 0, "In a worker, for resistFingerprinting: No entries for performance.getEntriesByType() for workers");
  is(performance.getEntriesByName("Test", "mark").length, 0, "In a worker, for resistFingerprinting: No entries for performance.getEntriesByName() for workers");

  finish();
}

function runRTPTests(expectedPrecision) {
  ok(isRounded(performance.timeOrigin, expectedPrecision), `In a worker, for reduceTimerPrecision, performance.timeOrigin is not correctly rounded: ` + performance.timeOrigin);

  // Try to add some entries.
  performance.mark("Test");
  performance.mark("Test-End");
  performance.measure("Test-Measure", "Test", "Test-End");

  // Check the entries in performance.getEntries/getEntriesByType/getEntriesByName.
  is(performance.getEntries().length, 3, "In a worker, for reduceTimerPrecision: Incorrect number of entries for performance.getEntries() for workers: " + performance.getEntries().length);
  for (var i = 0; i < 3; i++) {
    let startTime = performance.getEntries()[i].startTime;
    let duration = performance.getEntries()[i].duration;
    ok(isRounded(startTime, expectedPrecision), "In a worker, for reduceTimerPrecision(" + expectedPrecision + "), performance.getEntries(" + i.toString() + ").startTime is not rounded: " + startTime.toString());
    ok(isRounded(duration, expectedPrecision), "In a worker, for reduceTimerPrecision(" + expectedPrecision + "), performance.getEntries(" + i.toString() + ").duration is not rounded: " + duration.toString());
  }
  is(performance.getEntriesByType("mark").length, 2, "In a worker, for reduceTimerPrecision: Incorrect number of entries for performance.getEntriesByType() for workers: " + performance.getEntriesByType("resource").length);
  is(performance.getEntriesByName("Test", "mark").length, 1, "In a worker, for reduceTimerPrecision: Incorrect number of entries for performance.getEntriesByName() for workers: " + performance.getEntriesByName("Test", "mark").length);

  finish();
}

self.onmessage = function(e) {
  if (e.data.type === "runRPTests") {
    runRPTests(e.data.precision);
  } else if (e.data.type === "runRTPTests") {
    runRTPTests(e.data.precision);
  } else {
    ok(false, "Unknown message type");
  }
};
