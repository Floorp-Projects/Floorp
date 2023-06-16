/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test verifies that whether shortcut keys of toggling fullscreen modes
// are reserved.
add_task(async function test_keydown_event_reservation_toggling_fullscreen() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.transition-duration.enter", "0 0"],
      ["full-screen-api.transition-duration.leave", "0 0"],
    ],
  });

  let shortcutKeys = [{ key: "KEY_F11", modifiers: {} }];
  if (navigator.platform.startsWith("Mac")) {
    shortcutKeys.push({
      key: "f",
      modifiers: { metaKey: true, ctrlKey: true },
    });
    shortcutKeys.push({
      key: "F",
      modifiers: { metaKey: true, shiftKey: true },
    });
  }
  function shortcutDescription(aShortcutKey) {
    return `${
      aShortcutKey.metaKey ? "Meta + " : ""
    }${aShortcutKey.shiftKey ? "Shift + " : ""}${aShortcutKey.ctrlKey ? "Ctrl + " : ""}${aShortcutKey.key.replace("KEY_", "")}`;
  }
  for (const shortcutKey of shortcutKeys) {
    const tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.org/browser/browser/base/content/test/fullscreen/fullscreen.html"
    );

    await SimpleTest.promiseFocus(tab.linkedBrowser);

    const fullScreenEntered = BrowserTestUtils.waitForEvent(
      window,
      "fullscreen"
    );

    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      content.wrappedJSObject.keydown = null;
      content.window.addEventListener("keydown", event => {
        switch (event.key) {
          case "Shift":
          case "Meta":
          case "Control":
            break;
          default:
            content.wrappedJSObject.keydown = event;
        }
      });
    });

    EventUtils.synthesizeKey(shortcutKey.key, shortcutKey.modifiers);

    info(
      `Waiting for entering the fullscreen mode with synthesizing ${shortcutDescription(
        shortcutKey
      )}...`
    );
    await fullScreenEntered;

    info("Retrieving the result...");
    Assert.ok(
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        async () => !!content.wrappedJSObject.keydown
      ),
      `Entering the fullscreen mode with ${shortcutDescription(
        shortcutKey
      )} should cause "keydown" event`
    );

    const fullScreenExited = BrowserTestUtils.waitForEvent(
      window,
      "fullscreen"
    );

    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      content.wrappedJSObject.keydown = null;
    });

    EventUtils.synthesizeKey(shortcutKey.key, shortcutKey.modifiers);

    info(
      `Waiting for exiting from the fullscreen mode with synthesizing ${shortcutDescription(
        shortcutKey
      )}...`
    );
    await fullScreenExited;

    info("Retrieving the result...");
    Assert.ok(
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        async () => !content.wrappedJSObject.keydown
      ),
      `Exiting from the fullscreen mode with ${shortcutDescription(
        shortcutKey
      )} should not cause "keydown" event`
    );

    BrowserTestUtils.removeTab(tab);
  }
});
