/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Regression test for bug 1668838 - make sure that a popuphiding
 * event that fires for any popup not related to the device control
 * menus is ignored and doesn't cause the targets contents to be all
 * removed.
 */
add_task(async function test_popuphiding() {
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
      true /* camera */,
      true /* microphone */,
      SHARE_SCREEN
    );

    let indicator = await indicatorPromise;
    let doc = indicator.document;

    Assert.ok(doc.body, "Should have a document body in the indicator.");

    let event = new indicator.MouseEvent("popuphiding", { bubbles: true });
    doc.documentElement.dispatchEvent(event);

    Assert.ok(doc.body, "Should still have a document body in the indicator.");
  });

  await checkNotSharing();
});
