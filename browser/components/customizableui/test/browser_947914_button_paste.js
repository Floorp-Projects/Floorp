/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;
var globalClipboard;

add_task(function*() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, function*() {
    info("Check paste button existence and functionality");

    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    globalClipboard = Services.clipboard.kGlobalClipboard;

    yield PanelUI.show();
    info("Menu panel was opened");

    let pasteButton = document.getElementById("paste-button");
    ok(pasteButton, "Paste button exists in Panel Menu");

    // add text to clipboard
    let text = "Sample text for testing";
    clipboard.copyString(text);

    // test paste button by pasting text to URL bar
    gURLBar.focus();
    yield PanelUI.show();
    info("Menu panel was opened");

    ok(!pasteButton.hasAttribute("disabled"), "Paste button is enabled");
    pasteButton.click();

    is(gURLBar.value, text, "Text pasted successfully");
  });
});

registerCleanupFunction(function cleanup() {
  Services.clipboard.emptyClipboard(globalClipboard);
});
