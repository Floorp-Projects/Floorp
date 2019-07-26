/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.openViewOnFocus", true]],
  });

  // Add some history for the empty panel.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://mochi.test:8888/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);
  registerCleanupFunction(() => PlacesUtils.history.clear());

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      gURLBar.blur();
      gURLBar.focus();
      Assert.ok(!gURLBar.view.isOpen, "check urlbar panel is not open");
      Assert.ok(gURLBar.dropmarker.hidden, "The dropmarker should be hidden");
      gURLBar.blur();
      Assert.ok(gURLBar.dropmarker.hidden, "The dropmarker should be hidden");
      // Check the keyboard shortcut.
      await UrlbarTestUtils.promisePopupOpen(window, () => {
        window.document.getElementById("Browser:OpenLocation").doCommand();
      });
      gURLBar.blur();
      // Focus with the mouse.
      await UrlbarTestUtils.promisePopupOpen(window, () => {
        EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      });
      gURLBar.blur();
    }
  );
});
