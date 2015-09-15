"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

function test() {
  UITourTest();
}

var tests = [
  taskify(function*() {
    ok(!gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"), "Should not be in reader mode at start of test.");
    gContentAPI.toggleReaderMode();
    yield waitForConditionPromise(() => gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"));
    ok(gBrowser.selectedBrowser.currentURI.spec.startsWith("about:reader"), "Should be in reader mode now.");
  })
];
