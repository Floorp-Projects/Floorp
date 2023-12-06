/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kBaseUrlForContent = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const kContentFileName = "file_toplevel.html";
const kContentFileUrl = kBaseUrlForContent + kContentFileName;

async function waitForPasteContextMenu() {
  await waitForPasteMenuPopupEvent("shown");
  const pasteButton = document.getElementById(kPasteMenuItemId);
  info("Wait for paste button enabled");
  await BrowserTestUtils.waitForMutationCondition(
    pasteButton,
    { attributeFilter: ["disabled"] },
    () => !pasteButton.disabled,
    "Wait for paste button enabled"
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.events.asyncClipboard.readText", true],
      ["test.events.async.enabled", true],
      // Avoid paste button delay enabling making test too long.
      ["security.dialog_enable_delay", 0],
    ],
  });
});

add_task(async function test_multiple_readText_from_same_frame_allow() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const pasteButtonIsShown = waitForPasteContextMenu();
    const readTextRequest1 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    await pasteButtonIsShown;

    info("readText() from same frame again before interact with paste button");
    const readTextRequest2 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    // Give some time for the second request to arrive parent process.
    await SpecialPowers.spawn(browser, [], async () => {
      return new Promise(resolve => {
        content.setTimeout(resolve, 0);
      });
    });

    info("Click paste button, both request should be resolved");
    await promiseClickPasteButton();
    is(
      await readTextRequest1,
      clipboardText,
      "First request should be resolved"
    );
    is(
      await readTextRequest2,
      clipboardText,
      "Second request should be resolved"
    );
  });
});

add_task(async function test_multiple_readText_from_same_frame_deny() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const pasteButtonIsShown = waitForPasteContextMenu();
    const readTextRequest1 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    await pasteButtonIsShown;

    info("readText() from same frame again before interact with paste button");
    const readTextRequest2 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    // Give some time for the second request to arrive parent process.
    await SpecialPowers.spawn(browser, [], async () => {
      return new Promise(resolve => {
        content.setTimeout(resolve, 0);
      });
    });

    info("Dismiss paste button, both request should be rejected");
    await promiseDismissPasteButton();
    await Assert.rejects(
      readTextRequest1,
      /NotAllowedError/,
      "First request should be rejected"
    );
    await Assert.rejects(
      readTextRequest2,
      /NotAllowedError/,
      "Second request should be rejected"
    );
  });
});

add_task(async function test_multiple_readText_from_same_origin_frame() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const pasteButtonIsShown = waitForPasteContextMenu();
    const readTextRequest1 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    await pasteButtonIsShown;

    info(
      "readText() from same origin child frame again before interacting with paste button"
    );
    const sameOriginFrame = browser.browsingContext.children[0];
    const readTextRequest2 = SpecialPowers.spawn(
      sameOriginFrame,
      [],
      async () => {
        content.document.notifyUserGestureActivation();
        return content.eval(`navigator.clipboard.readText();`);
      }
    );
    // Give some time for the second request to arrive parent process.
    await SpecialPowers.spawn(sameOriginFrame, [], async () => {
      return new Promise(resolve => {
        content.setTimeout(resolve, 0);
      });
    });

    info("Click paste button, both request should be resolved");
    await promiseClickPasteButton();
    is(
      await readTextRequest1,
      clipboardText,
      "First request should be resolved"
    );
    is(
      await readTextRequest2,
      clipboardText,
      "Second request should be resolved"
    );
  });
});

add_task(async function test_multiple_readText_from_cross_origin_frame() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const pasteButtonIsShown = waitForPasteContextMenu();
    const readTextRequest1 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    await pasteButtonIsShown;

    info(
      "readText() from different origin child frame again before interacting with paste button"
    );
    const crossOriginFrame = browser.browsingContext.children[1];
    await Assert.rejects(
      SpecialPowers.spawn(crossOriginFrame, [], async () => {
        content.document.notifyUserGestureActivation();
        return content.eval(`navigator.clipboard.readText();`);
      }),
      /NotAllowedError/,
      "Second request should be rejected"
    );

    info("Click paste button, both request should be resolved");
    await promiseClickPasteButton();
    is(
      await readTextRequest1,
      clipboardText,
      "First request should be resolved"
    );
  });
});

add_task(async function test_multiple_readText_from_background_frame() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  const backgroundTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    kContentFileUrl
  );

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const pasteButtonIsShown = waitForPasteContextMenu();
    const readTextRequest1 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    await pasteButtonIsShown;

    info(
      "readText() from background tab again before interact with paste button"
    );
    await Assert.rejects(
      SpecialPowers.spawn(backgroundTab.linkedBrowser, [], async () => {
        content.document.notifyUserGestureActivation();
        return content.eval(`navigator.clipboard.readText();`);
      }),
      /NotAllowedError/,
      "Second request should be rejected"
    );

    info("Click paste button, both request should be resolved");
    await promiseClickPasteButton();
    is(
      await readTextRequest1,
      clipboardText,
      "First request should be resolved"
    );
  });

  await BrowserTestUtils.removeTab(backgroundTab);
});

add_task(async function test_multiple_readText_from_background_window() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const newWin = await BrowserTestUtils.openNewBrowserWindow();
    const backgroundTab = await BrowserTestUtils.openNewForegroundTab(
      newWin.gBrowser,
      kContentFileUrl
    );
    await SimpleTest.promiseFocus(browser);

    info("readText() from background window");
    await Assert.rejects(
      SpecialPowers.spawn(backgroundTab.linkedBrowser, [], async () => {
        content.document.notifyUserGestureActivation();
        return content.eval(`navigator.clipboard.readText();`);
      }),
      /NotAllowedError/,
      "Request from background window should be rejected"
    );

    const pasteButtonIsShown = waitForPasteContextMenu();
    const readTextRequest1 = SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    });
    await pasteButtonIsShown;

    info(
      "readText() from background window again before interact with paste button"
    );
    await Assert.rejects(
      SpecialPowers.spawn(backgroundTab.linkedBrowser, [], async () => {
        content.document.notifyUserGestureActivation();
        return content.eval(`navigator.clipboard.readText();`);
      }),
      /NotAllowedError/,
      "Second request should be rejected"
    );

    info("Click paste button, both request should be resolved");
    await promiseClickPasteButton();
    is(
      await readTextRequest1,
      clipboardText,
      "First request should be resolved"
    );

    await BrowserTestUtils.closeWindow(newWin);
  });
});

add_task(async function test_multiple_readText_focuse_in_chrome_document() {
  // Randomized text to avoid overlapping with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  const win = await BrowserTestUtils.openNewBrowserWindow();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    kContentFileUrl
  );

  info("Move focus to url bar");
  win.gURLBar.focus();

  info("readText() from web content");
  await Assert.rejects(
    SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      content.document.notifyUserGestureActivation();
      return content.eval(`navigator.clipboard.readText();`);
    }),
    /NotAllowedError/,
    "Request should be rejected when focus is not in content"
  );

  await BrowserTestUtils.closeWindow(win);
});
