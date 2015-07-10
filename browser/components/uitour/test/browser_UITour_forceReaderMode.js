"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

function test() {
  UITourTest();
}

let tests = [
  taskify(function*() {
    ok(!gBrowser.selectedBrowser.isArticle, "Should not be an article when we start");
    ok(document.getElementById("reader-mode-button").hidden, "Button should be hidden.");
    gContentAPI.forceShowReaderIcon();
    yield waitForConditionPromise(() => gBrowser.selectedBrowser.isArticle);
    ok(gBrowser.selectedBrowser.isArticle, "Should suddenly be an article.");
    ok(!document.getElementById("reader-mode-button").hidden, "Button should now be visible.");
  })
];

