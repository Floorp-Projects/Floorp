"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(function*() {
  ok(!gBrowser.selectedBrowser.isArticle, "Should not be an article when we start");
  ok(document.getElementById("reader-mode-button").hidden, "Button should be hidden.");
  yield gContentAPI.forceShowReaderIcon();
  yield waitForConditionPromise(() => gBrowser.selectedBrowser.isArticle);
  ok(gBrowser.selectedBrowser.isArticle, "Should suddenly be an article.");
  ok(!document.getElementById("reader-mode-button").hidden, "Button should now be visible.");
});

