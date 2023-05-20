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

const kContentFileName = "simple_navigator_clipboard_keydown.html";

const kContentFileUrl = kBaseUrlForContent + kContentFileName;

const kApzTestNativeEventUtilsUrl =
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js";

Services.scriptloader.loadSubScript(kApzTestNativeEventUtilsUrl, this);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.asyncClipboard.readText", true]],
  });
});

add_task(async function test_paste_button_clickjacking() {
  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    const pasteButtonIsShown = promisePasteButtonIsShown();

    // synthesize key to trigger readText() to bring up paste popup.
    EventUtils.synthesizeKey("p", {}, window);
    await waitForPasteMenuPopupEvent("shown");

    const pastePopup = document.getElementById(kPasteMenuPopupId);
    const pasteButton = document.getElementById(kPasteMenuItemId);
    ok(
      pasteButton.disabled,
      "Paste button should be shown with disabled by default"
    );

    let accesskey = pasteButton.getAttribute("accesskey");
    let delay = Services.prefs.getIntPref("security.dialog_enable_delay") * 3;
    while (delay > 0) {
      // There's no other way to allow some time to pass and ensure we're
      // genuinely testing that these keypresses postpone the enabling of
      // the paste button, so disable this check for this line:
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, 100));
      ok(pasteButton.disabled, "Paste button should still be disabled");
      EventUtils.synthesizeKey(accesskey, {}, window);
      is(pastePopup.state, "open", "Paste popup should still be opened");
      delay = delay - 100;
    }

    await BrowserTestUtils.waitForMutationCondition(
      pasteButton,
      { attributeFilter: ["disabled"] },
      () => !pasteButton.disabled,
      "Wait for paste button enabled"
    );

    const pasteButtonIsHidden = promisePasteButtonIsHidden();
    EventUtils.synthesizeKey(accesskey, {}, window);
    await pasteButtonIsHidden;
  });
});
