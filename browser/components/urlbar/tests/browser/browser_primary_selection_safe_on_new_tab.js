/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verify that the primary selection is unaffected by opening a new tab.
 *
 * The steps here follow STR for regression
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1457355.
 */

"use strict";

let tabs = [];
let supportsPrimary = Services.clipboard.supportsSelectionClipboard();
const NON_EMPTY_URL = "data:text/html,Hello";
const TEXT_FOR_PRIMARY = "Text for PRIMARY selection";

add_task(async function() {
  tabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, NON_EMPTY_URL)
  );

  // Bug 1457355 reproduced only when the url had a non-empty selection.
  gURLBar.select();
  Assert.equal(gURLBar.inputField.selectionStart, 0);
  Assert.equal(
    gURLBar.inputField.selectionEnd,
    gURLBar.inputField.value.length
  );

  if (supportsPrimary) {
    clipboardHelper.copyStringToClipboard(
      TEXT_FOR_PRIMARY,
      Services.clipboard.kSelectionClipboard
    );
  }

  tabs.push(
    await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: () => {
        // Simulate tab open from user input such as keyboard shortcut or new
        // tab button.
        let userInput = window.windowUtils.setHandlingUserInput(true);
        try {
          BrowserOpenTab();
        } finally {
          userInput.destruct();
        }
      },
      waitForLoad: false,
    })
  );

  if (!supportsPrimary) {
    info("Primary selection not supported. Skipping assertion.");
    return;
  }

  let primaryAsText = SpecialPowers.getClipboardData(
    "text/unicode",
    SpecialPowers.Ci.nsIClipboard.kSelectionClipboard
  );
  Assert.equal(primaryAsText, TEXT_FOR_PRIMARY);
});

registerCleanupFunction(() => {
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
