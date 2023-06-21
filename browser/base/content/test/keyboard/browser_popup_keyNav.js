/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);

/**
 * Keyboard navigation has some edgecases in popups because
 * there is no tabstrip or menubar. Check that tabbing forward
 * and backward to/from the content document works:
 */
add_task(async function test_popup_keynav() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.toolbars.keyboard_navigation", true],
      ["accessibility.tabfocus", 7],
    ],
  });

  const kURL = TEST_PATH + "focusableContent.html";
  await BrowserTestUtils.withNewTab(kURL, async browser => {
    let windowPromise = BrowserTestUtils.waitForNewWindow({
      url: kURL,
    });
    SpecialPowers.spawn(browser, [], () => {
      content.window.open(
        content.location.href,
        "_blank",
        "height=500,width=500,menubar=no,toolbar=no,status=1,resizable=1"
      );
    });
    let win = await windowPromise;
    let hamburgerButton = win.document.getElementById("PanelUI-menu-button");
    await focusAndActivateElement(hamburgerButton, () =>
      expectFocusAfterKey("Tab", win.gBrowser.selectedBrowser, false, win)
    );
    // Focus the button inside the webpage.
    EventUtils.synthesizeKey("KEY_Tab", {}, win);
    // Focus the first item in the URL bar
    let firstButton = win.document
      .getElementById("urlbar-container")
      .querySelector("toolbarbutton,[role=button]");
    await expectFocusAfterKey("Tab", firstButton, false, win);
    await BrowserTestUtils.closeWindow(win);
  });
});
