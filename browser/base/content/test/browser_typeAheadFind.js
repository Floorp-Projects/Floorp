/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let testWindow = null;

function test() {
  waitForExplicitFinish();

  testWindow = OpenBrowserWindow();
  whenDelayedStartupFinished(testWindow, function () {
    let selectedBrowser = testWindow.gBrowser.selectedBrowser;
    selectedBrowser.addEventListener("pageshow", function() {
      selectedBrowser.removeEventListener("pageshow", arguments.callee, true);
      ok(true, "pageshow listener called");
      waitForFocus(onFocus, testWindow.content);
    }, true);
    gBrowser.loadURI("data:text/html,<h1>A Page</h1>");
  });
}

function onFocus() {
  ok(!testWindow.gFindBarInitialized, "find bar is not initialized");
  EventUtils.synthesizeKey("/", {}, testWindow);
  ok(testWindow.gFindBarInitialized, "find bar is now initialized");
  testWindow.close();
  finish();
}
