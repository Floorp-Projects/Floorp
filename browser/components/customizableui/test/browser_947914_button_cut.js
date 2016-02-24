/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;
var globalClipboard;

add_task(function*() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, function*() {
    info("Check cut button existence and functionality");

    let testText = "cut text test";

    gURLBar.focus();
    yield PanelUI.show();
    info("Menu panel was opened");

    let cutButton = document.getElementById("cut-button");
    ok(cutButton, "Cut button exists in Panel Menu");
    ok(cutButton.hasAttribute("disabled"), "Cut button is disabled");

    // cut text from URL bar
    gURLBar.value = testText;
    gURLBar.focus();
    gURLBar.select();
    yield PanelUI.show();
    info("Menu panel was opened");

    ok(!cutButton.hasAttribute("disabled"), "Cut button is enabled when selecting");
    cutButton.click();
    is(gURLBar.value, "", "Selected text is removed from source when clicking on cut");

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
});

registerCleanupFunction(function cleanup() {
  Services.clipboard.emptyClipboard(globalClipboard);
});
