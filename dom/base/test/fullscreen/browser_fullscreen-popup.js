/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function waitForPopup(aPopup, aOpened, aMsg) {
  await BrowserTestUtils.waitForPopupEvent(
    aPopup,
    aOpened ? "shown" : "hidden"
  );
  Assert.equal(
    aPopup.popupOpen,
    aOpened,
    `Check ${aMsg} popup is ${aOpened ? "opened" : "closed"}`
  );
}

async function waitForFullscreen(aWindow, aFullscreen) {
  let promise = BrowserTestUtils.waitForEvent(aWindow, "fullscreen");
  aWindow.fullScreen = aFullscreen;
  await promise;
  Assert.equal(aWindow.fullScreen, aFullscreen, `Check fullscreen state`);
}

function testFullscreenPopup(aURL, aPopId, aOpenPopupFun) {
  add_task(async function test_fullscreen_popup() {
    info(`Tests for ${aPopId}`);
    const win = await BrowserTestUtils.openNewBrowserWindow();
    const tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, aURL);

    const browser = tab.linkedBrowser;
    const popup = win.document.getElementById(aPopId);

    info("Show popup");
    let shownPromise = waitForPopup(popup, true, aPopId);
    await aOpenPopupFun(browser);
    await shownPromise;

    info("Popup should be closed after entering fullscreen");
    let hiddenPromise = waitForPopup(popup, false, aPopId);
    await waitForFullscreen(win, true);
    await hiddenPromise;

    info("Show popup again");
    shownPromise = waitForPopup(popup, true, aPopId);
    await aOpenPopupFun(browser);
    await shownPromise;

    info("Popup should be closed after exiting fullscreen");
    hiddenPromise = waitForPopup(popup, false, aPopId);
    await waitForFullscreen(win, false);
    await hiddenPromise;

    // Close opened tab
    let tabClosed = BrowserTestUtils.waitForTabClosing(tab);
    await BrowserTestUtils.removeTab(tab);
    await tabClosed;

    // Close opened window
    await BrowserTestUtils.closeWindow(win);
  });
}

// Test for autocomplete.
testFullscreenPopup(
  `data:text/html,
     <input id="list-input" list="test-list" type="text"/>
     <datalist id="test-list">
        <option value="item 1"/>
        <option value="item 2"/>
        <option value="item 3"/>
        <option value="item 4"/>
      </datalist>`,
  "PopupAutoComplete",
  async browser => {
    await SpecialPowers.spawn(browser, [], async function () {
      const input = content.document.querySelector("input");
      input.focus();
    });
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  }
);
