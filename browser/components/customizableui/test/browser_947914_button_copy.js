/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;
var globalClipboard;

add_task(async function() {
  await BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, async function() {
    info("Check copy button existence and functionality");
    CustomizableUI.addWidgetToArea("edit-controls", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

    await waitForOverflowButtonShown();

    let testText = "copy text test";

    gURLBar.focus();
    info("The URL bar was focused");
    await document.getElementById("nav-bar").overflowable.show();
    info("Menu panel was opened");

    let copyButton = document.getElementById("copy-button");
    ok(copyButton, "Copy button exists in Panel Menu");
    ok(copyButton.getAttribute("disabled"), "Copy button is initially disabled");

    // copy text from URL bar
    gURLBar.value = testText;
    gURLBar.focus();
    gURLBar.select();
    await document.getElementById("nav-bar").overflowable.show();
    info("Menu panel was opened");

    ok(!copyButton.hasAttribute("disabled"), "Copy button is enabled when selecting");

    await SimpleTest.promiseClipboardChange(testText, () => {
      copyButton.click();
    });

    is(gURLBar.value, testText, "Selected text is unaltered when clicking copy");
  });
});

registerCleanupFunction(function cleanup() {
  CustomizableUI.reset();
});
