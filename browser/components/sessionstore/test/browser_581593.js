/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let stateBackup = ss.getBrowserState();

function test() {
  /** Test for bug 581593 **/
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  let oldState = { windows: [{ tabs: [{ entries: [{ url: "example.com" }] }] }]};
  let pageData = {
    url: "about:sessionrestore",
    formdata: { id: { "sessionData": "(" + JSON.stringify(oldState) + ")" } }
  };
  let state = { windows: [{ tabs: [{ entries: [pageData] }] }] };

  // The form data will be restored before SSTabRestored, so we want to listen
  // for that on the currently selected tab (it will be reused)
  gBrowser.selectedTab.addEventListener("SSTabRestored", onSSTabRestored, true);

  ss.setBrowserState(JSON.stringify(state));
}

function onSSTabRestored(aEvent) {
  info("SSTabRestored event");
  gBrowser.selectedTab.removeEventListener("SSTabRestored", onSSTabRestored, true);
  gBrowser.selectedBrowser.addEventListener("input", onInput, true);
}

function onInput(aEvent) {
  info("input event");
  gBrowser.selectedBrowser.removeEventListener("input", onInput, true);

  // This is an ok way to check this because we will make sure that the text
  // field is parsable.
  let val = gBrowser.selectedBrowser.contentDocument.getElementById("sessionData").value;
  try {
    JSON.parse(val);
    ok(true, "JSON.parse succeeded");
  }
  catch (e) {
    ok(false, "JSON.parse failed");
  }
  cleanup();
}

function cleanup() {
  ss.setBrowserState(stateBackup);
  executeSoon(finish);
}
