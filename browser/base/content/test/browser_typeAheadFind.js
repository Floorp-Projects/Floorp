/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let testWindow = null;

function test() {
  waitForExplicitFinish();

  testWindow = OpenBrowserWindow();
  testWindow.addEventListener("load", function() {
    testWindow.removeEventListener("load", arguments.callee, false);
    ok(true, "Load listener called");

    executeSoon(function() {
      let selectedBrowser = testWindow.gBrowser.selectedBrowser;
      selectedBrowser.addEventListener("pageshow", function() {
        selectedBrowser.removeEventListener("pageshow", arguments.callee,
                                            false);
        ok(true, "pageshow listener called");
        waitForFocus(onFocus, testWindow.content);
      }, true);
      testWindow.content.location = "data:text/html,<h1>A Page</h1>";
    });
  }, false);
}

function onFocus() {
  EventUtils.synthesizeKey("/", {});
  ok(gFindBarInitialized, "find bar is now initialized");
  testWindow.gFindBar.close();
  testWindow.close();
  finish();
}
