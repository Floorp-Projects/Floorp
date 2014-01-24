/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check paste button existence and functionality");
  let initialLocation = gBrowser.currentURI.spec;
  let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);

  yield PanelUI.show();

  let pasteButton = document.getElementById("paste-button");
  ok(pasteButton, "Paste button exists in Panel Menu");

  // add text to clipboard
  var text = "Sample text for testing";
  clipboard.copyString(text);

  // test paste button by pasting text to URL bar
  gURLBar.focus();
  yield PanelUI.show();
  ok(!pasteButton.hasAttribute("disabled"), "Paste button is enabled");
  pasteButton.click();

  is(gURLBar.value, text, "Text pasted successfully");

  // restore the tab as it was at the begining of the test
  var tabToRemove = gBrowser.selectedTab;
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(tabToRemove);
});
