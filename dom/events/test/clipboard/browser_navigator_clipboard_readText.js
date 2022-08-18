/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head.js */

const kBaseUrlForContent = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const kContentFileName = "simple_navigator_clipboard_readText.html";

const kContentFileUrl = kBaseUrlForContent + kContentFileName;

const kApzTestNativeEventUtilsUrl =
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js";

Services.scriptloader.loadSubScript(kApzTestNativeEventUtilsUrl, this);

const chromeDoc = window.document;

// @param aBrowser browser object of the content tab.
// @param aMultipleReadTextCalls if false, exactly one call is made, two
//                               otherwise.
function promiseClickContentToTriggerClipboardReadText(
  aBrowser,
  aMultipleReadTextCalls
) {
  const contentButtonId = aMultipleReadTextCalls
    ? "invokeReadTextTwiceId"
    : "invokeReadTextOnceId";

  // `aBrowser.contentDocument` is null, therefore use `SpecialPowers.spawn`.
  return SpecialPowers.spawn(
    aBrowser,
    [contentButtonId],
    async _contentButtonId => {
      const contentButton = content.document.getElementById(_contentButtonId);
      let promise = new Promise(resolve => {
        contentButton.addEventListener(
          "click",
          function(e) {
            resolve({ x: e.screenX, y: e.screenY });
          },
          { once: true }
        );
      });

      EventUtils.synthesizeMouseAtCenter(contentButton, {}, content.window);

      return promise;
    }
  );
}

// @param aBrowser browser object of the content tab.
function promiseMutatedReadTextResultFromContentElement(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], async () => {
    const readTextResultElement = content.document.getElementById(
      "readTextResultId"
    );

    const promiseReadTextResult = new Promise(resolve => {
      const mutationObserver = new content.MutationObserver(
        (aMutationRecord, aMutationObserver) => {
          info("Observed mutation.");
          aMutationObserver.disconnect();
          resolve(readTextResultElement.textContent);
        }
      );

      mutationObserver.observe(readTextResultElement, {
        childList: true,
      });
    });

    return await promiseReadTextResult;
  });
}

function promiseWritingRandomTextToClipboard() {
  const clipboardText = "X" + Math.random();
  return navigator.clipboard.writeText(clipboardText).then(() => {
    return clipboardText;
  });
}

function promiseDismissPasteButton() {
  // nsXULPopupManager rollup is handled in widget code, so we have to
  // synthesize native mouse events.
  return EventUtils.promiseNativeMouseEvent({
    type: "click",
    target: chromeDoc.body,
    // Relies on the assumption that the center of chrome document doesn't
    // overlay with the paste button showed for clipboard readText request.
    atCenter: true,
  });
}

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.events.asyncClipboard.readText", true],
      ["test.events.async.enabled", true],
    ],
  });
});

add_task(async function test_paste_button_position() {
  // Ensure there's text on the clipboard.
  await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function(browser) {
    const pasteButtonIsShown = promisePasteButtonIsShown();
    const coordsOfClickInContentRelativeToScreenInDevicePixels = await promiseClickContentToTriggerClipboardReadText(
      browser,
      false
    );
    info(
      "coordsOfClickInContentRelativeToScreenInDevicePixels: " +
        coordsOfClickInContentRelativeToScreenInDevicePixels.x +
        ", " +
        coordsOfClickInContentRelativeToScreenInDevicePixels.y
    );

    const pasteButtonCoordsRelativeToScreenInDevicePixels = await pasteButtonIsShown;
    info(
      "pasteButtonCoordsRelativeToScreenInDevicePixels: " +
        pasteButtonCoordsRelativeToScreenInDevicePixels.x +
        ", " +
        pasteButtonCoordsRelativeToScreenInDevicePixels.y
    );

    const mouseCoordsRelativeToScreenInDevicePixels = getMouseCoordsRelativeToScreenInDevicePixels();
    info(
      "mouseCoordsRelativeToScreenInDevicePixels: " +
        mouseCoordsRelativeToScreenInDevicePixels.x +
        ", " +
        mouseCoordsRelativeToScreenInDevicePixels.y
    );

    // Asserting not overlapping is important; otherwise, when the
    // "Paste" button is shown via a `mousedown` event, the following
    // `mouseup` event could accept the "Paste" button unnoticed by the
    // user.
    ok(
      isCloselyLeftOnTopOf(
        mouseCoordsRelativeToScreenInDevicePixels,
        pasteButtonCoordsRelativeToScreenInDevicePixels
      ),
      "'Paste' button is closely left on top of the mouse pointer."
    );
    ok(
      isCloselyLeftOnTopOf(
        coordsOfClickInContentRelativeToScreenInDevicePixels,
        pasteButtonCoordsRelativeToScreenInDevicePixels
      ),
      "Coords of click in content are closely left on top of the 'Paste' button."
    );

    // To avoid disturbing subsequent tests.
    const pasteButtonIsHidden = promisePasteButtonIsHidden();
    await promiseClickPasteButton();
    await pasteButtonIsHidden;
  });
});

add_task(async function test_accepting_paste_button() {
  // Randomized text to avoid overlappings with other tests.
  const clipboardText = await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function(browser) {
    const pasteButtonIsShown = promisePasteButtonIsShown();
    await promiseClickContentToTriggerClipboardReadText(browser, false);
    await pasteButtonIsShown;
    const pasteButtonIsHidden = promisePasteButtonIsHidden();
    const mutatedReadTextResultFromContentElement = promiseMutatedReadTextResultFromContentElement(
      browser
    );
    await promiseClickPasteButton();
    await pasteButtonIsHidden;
    await mutatedReadTextResultFromContentElement.then(value => {
      is(
        value,
        "Resolved: " + clipboardText,
        "Text returned from `navigator.clipboard.readText()` is as expected."
      );
    });
  });
});

add_task(async function test_dismissing_paste_button() {
  await BrowserTestUtils.withNewTab(kContentFileUrl, async function(browser) {
    const pasteButtonIsShown = promisePasteButtonIsShown();
    await promiseClickContentToTriggerClipboardReadText(browser, false);
    await pasteButtonIsShown;
    const pasteButtonIsHidden = promisePasteButtonIsHidden();
    const mutatedReadTextResultFromContentElement = promiseMutatedReadTextResultFromContentElement(
      browser
    );
    await promiseDismissPasteButton();
    await pasteButtonIsHidden;
    await mutatedReadTextResultFromContentElement.then(value => {
      is(
        value,
        "Rejected.",
        "`navigator.clipboard.readText()` rejected after dismissing the 'Paste' button"
      );
    });
  });
});

add_task(
  async function test_multiple_readText_invocations_for_same_user_activation() {
    // Randomized text to avoid overlappings with other tests.
    const clipboardText = await promiseWritingRandomTextToClipboard();

    await BrowserTestUtils.withNewTab(kContentFileUrl, async function(browser) {
      const pasteButtonIsShown = promisePasteButtonIsShown();
      await promiseClickContentToTriggerClipboardReadText(browser, true);
      await pasteButtonIsShown;
      const mutatedReadTextResultFromContentElement = promiseMutatedReadTextResultFromContentElement(
        browser
      );
      const pasteButtonIsHidden = promisePasteButtonIsHidden();
      await promiseClickPasteButton();
      await mutatedReadTextResultFromContentElement.then(value => {
        is(
          value,
          "Resolved 1: " + clipboardText + "; Resolved 2: " + clipboardText,
          "Two calls of `navigator.clipboard.read()` both resolved with the expected text."
        );
      });

      // To avoid disturbing subsequent tests.
      await pasteButtonIsHidden;
    });
  }
);

add_task(async function test_new_user_activation_shows_paste_button_again() {
  await BrowserTestUtils.withNewTab(kContentFileUrl, async function(browser) {
    // Ensure there's text on the clipboard.
    await promiseWritingRandomTextToClipboard();

    for (let i = 0; i < 2; ++i) {
      const pasteButtonIsShown = promisePasteButtonIsShown();
      // A click initiates a new user activation.
      await promiseClickContentToTriggerClipboardReadText(browser, false);
      await pasteButtonIsShown;

      const pasteButtonIsHidden = promisePasteButtonIsHidden();
      await promiseClickPasteButton();
      await pasteButtonIsHidden;
    }
  });
});
