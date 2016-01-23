/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;
var globalClipboard;

add_task(function*() {
  info("Check copy button existence and functionality");

  let testText = "copy text test";

  gURLBar.focus();
  info("The URL bar was focused");
  yield PanelUI.show();
  info("Menu panel was opened");

  let copyButton = document.getElementById("copy-button");
  ok(copyButton, "Copy button exists in Panel Menu");
  ok(copyButton.getAttribute("disabled"), "Copy button is initially disabled");

  // copy text from URL bar
  gURLBar.value = testText;
  gURLBar.focus();
  gURLBar.select();
  yield PanelUI.show();
  info("Menu panel was opened");

  ok(!copyButton.hasAttribute("disabled"), "Copy button is enabled when selecting");

  copyButton.click();
  is(gURLBar.value, testText, "Selected text is unaltered when clicking copy");

  // check that the text was added to the clipboard
  let clipboard = Services.clipboard;
  let transferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(Ci.nsITransferable);
  globalClipboard = clipboard.kGlobalClipboard;

  transferable.init(null);
  transferable.addDataFlavor("text/unicode");
  clipboard.getData(transferable, globalClipboard);
  let str = {}, strLength = {};
  transferable.getTransferData("text/unicode", str, strLength);
  let clipboardValue = "";

  if (str.value) {
    str.value.QueryInterface(Ci.nsISupportsString);
    clipboardValue = str.value.data;
  }
  is(clipboardValue, testText, "Data was copied to the clipboard.");
});

add_task(function* asyncCleanup() {
  // clear the clipboard
  Services.clipboard.emptyClipboard(globalClipboard);
  info("Clipboard was cleared");

  // restore the tab as it was at the begining of the test
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(gBrowser.selectedTab);
  info("Tabs were restored");
});
