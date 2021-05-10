"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_UITour_task(async function() {
  ok(
    !gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"),
    "Should not be in reader mode at start of test."
  );
  await gContentAPI.toggleReaderMode();
  await waitForConditionPromise(() =>
    gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader")
  );
  ok(
    gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"),
    "Should be in reader mode now."
  );
});
