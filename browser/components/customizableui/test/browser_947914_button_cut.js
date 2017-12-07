/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;
var globalClipboard;

add_task(async function() {
  await BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, async function() {
    info("Check cut button existence and functionality");
    CustomizableUI.addWidgetToArea("edit-controls", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

    await waitForOverflowButtonShown();

    let testText = "cut text test";

    gURLBar.focus();
    await document.getElementById("nav-bar").overflowable.show();
    info("Menu panel was opened");

    let cutButton = document.getElementById("cut-button");
    ok(cutButton, "Cut button exists in Panel Menu");
    ok(cutButton.hasAttribute("disabled"), "Cut button is disabled");

    // cut text from URL bar
    gURLBar.value = testText;
    gURLBar.focus();
    gURLBar.select();
    await document.getElementById("nav-bar").overflowable.show();
    info("Menu panel was opened");

    ok(!cutButton.hasAttribute("disabled"), "Cut button is enabled when selecting");
    await SimpleTest.promiseClipboardChange(testText, () => {
      cutButton.click();
    });
    is(gURLBar.value, "", "Selected text is removed from source when clicking on cut");
  });
});

registerCleanupFunction(function cleanup() {
  CustomizableUI.reset();
});
