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
const kContentFileUrl =
  kBaseUrlForContent + "simple_navigator_clipboard_readText.html";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

// @param aBrowser browser object of the content tab.
// @param aContentElementId the ID of the element to be tapped.
function promiseTouchTapContent(aBrowser, aContentElementId) {
  return SpecialPowers.spawn(
    aBrowser,
    [aContentElementId],
    async _contentElementId => {
      await content.wrappedJSObject.waitUntilApzStable();

      const contentElement = content.document.getElementById(_contentElementId);
      let promise = new Promise(resolve => {
        contentElement.addEventListener(
          "click",
          function(e) {
            resolve({ x: e.screenX, y: e.screenY });
          },
          { once: true }
        );
      });

      EventUtils.synthesizeTouchAtCenter(contentElement, {}, content.window);

      return promise;
    }
  );
}

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.events.asyncClipboard.readText", true],
      ["test.events.async.enabled", true],
    ],
  });
});

add_task(async function test_paste_button_position_touch() {
  // Ensure there's text on the clipboard.
  await promiseWritingRandomTextToClipboard();

  await BrowserTestUtils.withNewTab(kContentFileUrl, async function(browser) {
    const pasteButtonIsShown = promisePasteButtonIsShown();
    const coordsOfClickInContentRelativeToScreenInDevicePixels = await promiseTouchTapContent(
      browser,
      "invokeReadTextOnceId"
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
