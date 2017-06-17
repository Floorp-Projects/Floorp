function ok(a, msg) {
  postMessage({type: "status", status: !!a, msg});
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({type: "finish"});
}

function runTests() {
  // Try to add some entries.
  performance.mark("Test");
  performance.mark("Test-End");
  performance.measure("Test-Measure", "Test", "Test-End");

  // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
  is(performance.getEntries().length, 0, "No entries for performance.getEntries() for workers");
  is(performance.getEntriesByType("resource").length, 0, "No entries for performance.getEntriesByType() for workers");
  is(performance.getEntriesByName("Test", "mark").length, 0, "No entries for performance.getEntriesByName() for workers");

  finish();
}

self.onmessage = function(e) {
  if (e.data.type === "runTests") {
    runTests();
  } else {
    ok(false, "Unknown message type");
  }
}
