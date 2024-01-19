/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for the presence of selected action text "Extensions:" in the URL bar.
 */

add_task(async function testSwitchToTabTextDisplay() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      omnibox: {
        keyword: "omniboxtest",
      },

      background() {
        /* global browser */
        browser.omnibox.setDefaultSuggestion({
          description: "doit",
        });
        // Just do nothing for this test.
      },
    },
  });

  await extension.startup();

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "omniboxtest ",
    fireInputEvent: true,
  });

  // The "Extension:" label appears after a key down followed by a key up
  // back to the extension result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowUp");

  // Checks to see if "Extension:" text in URL bar is visible
  const extensionText = document.getElementById("urlbar-label-extension");
  Assert.ok(BrowserTestUtils.isVisible(extensionText));
  Assert.equal(extensionText.value, "Extension:");

  // Check to see if all other labels are hidden
  const allLabels = document.getElementById("urlbar-label-box").children;
  for (let label of allLabels) {
    if (label != extensionText) {
      Assert.ok(BrowserTestUtils.isHidden(label));
    }
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
  await extension.unload();
});
