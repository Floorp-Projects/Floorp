"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(function*() {
  ok(!gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"),
     "Should not be in reader mode at start of test.");
  yield gContentAPI.toggleReaderMode();
  yield waitForConditionPromise(() => gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"));
  ok(gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"),
     "Should be in reader mode now.");
});
