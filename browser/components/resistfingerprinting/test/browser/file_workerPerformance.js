function ok(a, msg) {
  postMessage({type: "status", status: !!a, msg});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({type: "finish"});
}

function runRPTests() {
  const isRounded = x => (Math.floor(x / 100) * 100) === x;
  //ok(isRounded(performance.timeOrigin), `For resistFingerprinting, performance.timeOrigin is not correctly rounded: ` + performance.timeOrigin);

  // Try to add some entries.
  performance.mark("Test");
  performance.mark("Test-End");
  performance.measure("Test-Measure", "Test", "Test-End");

  // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
  is(performance.getEntries().length, 0, "For resistFingerprinting: No entries for performance.getEntries() for workers");
  is(performance.getEntriesByType("resource").length, 0, "For resistFingerprinting: No entries for performance.getEntriesByType() for workers");
  is(performance.getEntriesByName("Test", "mark").length, 0, "For resistFingerprinting: No entries for performance.getEntriesByName() for workers");

  finish();
}

function runRTPTests() {
  const isRounded = x => (Math.floor(x / 100) * 100) === x;
  //ok(isRounded(performance.timeOrigin), `For reduceTimerPrecision, performance.timeOrigin is not correctly rounded: ` + performance.timeOrigin);

  // Try to add some entries.
  performance.mark("Test");
  performance.mark("Test-End");
  performance.measure("Test-Measure", "Test", "Test-End");

  // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
  is(performance.getEntries().length, 3, "For reduceTimerPrecision: Incorrect number of entries for performance.getEntries() for workers: " + performance.getEntries().length);
  for(var i=0; i<3; i++) {
    let startTime = performance.getEntries()[i].startTime;
    let duration = performance.getEntries()[i].duration;
    ok(isRounded(startTime), "For reduceTimerPrecision, performance.getEntries(" + i.toString() + ").startTime is not rounded: " + startTime.toString());
    ok(isRounded(duration), "For reduceTimerPrecision, performance.getEntries(" + i.toString() + ").duration is not rounded: " + duration.toString());
  }
  is(performance.getEntriesByType("mark").length, 2, "For reduceTimerPrecision: Incorrect number of entries for performance.getEntriesByType() for workers: " + performance.getEntriesByType("resource").length);
  is(performance.getEntriesByName("Test", "mark").length, 1, "For reduceTimerPrecision: Incorrect number of entries for performance.getEntriesByName() for workers: " + performance.getEntriesByName("Test", "mark").length);

  finish();
}

self.onmessage = function(e) {
  if (e.data.type === "runRPTests") {
    runRPTests();
  } else if (e.data.type === "runRTPTests") {
    runRTPTests();
  } else {
    ok(false, "Unknown message type");
  }
};
