"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_UITour_task(async function() {
  ok(
    !gBrowser.selectedBrowser.isArticle,
    "Should not be an article when we start"
  );
  ok(
    document.getElementById("reader-mode-button").hidden,
    "Button should be hidden."
  );
  await gContentAPI.forceShowReaderIcon();
  await waitForConditionPromise(() => gBrowser.selectedBrowser.isArticle);
  ok(gBrowser.selectedBrowser.isArticle, "Should suddenly be an article.");
  ok(
    !document.getElementById("reader-mode-button").hidden,
    "Button should now be visible."
  );
});
