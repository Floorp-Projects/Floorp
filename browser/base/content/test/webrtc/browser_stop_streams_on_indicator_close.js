/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Tests that if the indicator is closed somehow by the user when streams
 * still ongoing, that all of those streams are stopped, and the most recent
 * tab that a stream was shared with is selected.
 */
add_task(async function test_close_indicator() {
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  let indicatorPromise = promiseIndicatorWindow();

  info("Opening first tab");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing camera, microphone and screen");
  await shareDevices(tab1.linkedBrowser, true, true, true);

  info("Opening second tab");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing camera and screen");
  await shareDevices(tab2.linkedBrowser, true, false, true);

  info("Opening third tab");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing screen");
  await shareDevices(tab3.linkedBrowser, false, false, true);

  info("Opening fourth tab");
  let tab4 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  Assert.equal(
    gBrowser.selectedTab,
    tab4,
    "Most recently opened tab is selected"
  );

  let indicator = await indicatorPromise;

  indicator.close();

  await checkNotSharing();

  Assert.equal(
    webrtcUI.activePerms.size,
    0,
    "There shouldn't be any active stream permissions."
  );

  Assert.equal(
    gBrowser.selectedTab,
    tab3,
    "Most recently tab that streams were shared with is selected"
  );
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
});
