/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Regression test for bug 1669801, where sharing a window would
 * result in a device control menu that showed the wrong count.
 */
add_task(async function test_bug_1669801() {
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let indicatorPromise = promiseIndicatorWindow();

    await shareDevices(
      browser,
      false /* camera */,
      false /* microphone */,
      SHARE_WINDOW
    );

    let indicator = await indicatorPromise;
    let doc = indicator.document;

    let menupopup = doc.querySelector("menupopup[type='Screen']");
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      menupopup,
      "popupshown"
    );
    menupopup.openPopup(doc.body, {});
    await popupShownPromise;

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      menupopup,
      "popuphidden"
    );
    menupopup.hidePopup();
    await popupHiddenPromise;
    await closeStream();
  });
});
