/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let gTestTab;
let gContentAPI;
let gContentWindow;

Cu.import("resource:///modules/UITour.jsm");

function test() {
  UITourTest();
}

let tests = [
  taskify(function* test_openPreferences() {
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences");
    gContentAPI.openPreferences();
    let tab = yield promiseTabOpened;
    yield BrowserTestUtils.removeTab(tab);
  }),

  taskify(function* test_openInvalidPreferences() {
    gContentAPI.openPreferences(999);

    try {
      yield waitForConditionPromise(() => {
        return gBrowser.selectedBrowser.currentURI.spec.startsWith("about:preferences");
      }, "Check if about:preferences opened");
      ok(false, "No about:preferences tab should have opened");
    } catch (ex) {
      ok(true, "No about:preferences tab opened: " + ex);
    }
  }),

  taskify(function* test_openPrivacyPreferences() {
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy");
    gContentAPI.openPreferences("privacy");
    let tab = yield promiseTabOpened;
    yield BrowserTestUtils.removeTab(tab);
  }),
];
