/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();
  ok(true, "Starting up");

  gBrowser.selectedBrowser.focus();
  gURLBar.addEventListener("focus", function onFocus() {
    gURLBar.removeEventListener("focus", onFocus);
    ok(true, "Invoked onfocus handler");
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true });

    // javscript: URIs are evaluated async.
    SimpleTest.executeSoon(function() {
      ok(true, "Evaluated without crashing");
      finish();
    });
  });
  gURLBar.inputField.value = "javascript: var foo = '11111111'; ";
  gURLBar.focus();
}
