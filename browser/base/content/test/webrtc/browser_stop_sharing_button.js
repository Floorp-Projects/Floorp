/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

add_task(async function setup() {
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });
});

/**
 * Tests that if the user chooses to "Stop Sharing" a display while
 * also sharing their microphone or camera, that only the display
 * stream is stopped.
 */
add_task(async function test_stop_sharing() {
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
      true /* screen */
    );

    let indicator = await indicatorPromise;

    let stopSharingButton = indicator.document.getElementById("stop-sharing");
    let stopSharingPromise = expectObserverCalled("recording-device-events");
    stopSharingButton.click();
    await stopSharingPromise;

    // Ensure that we're still sharing the other streams.
    await checkSharingUI({ audio: true, video: true });

    // Ensure that the "display-share" section of the indicator is now hidden
    Assert.ok(
      BrowserTestUtils.is_hidden(
        indicator.document.getElementById("display-share")
      ),
      "The display-share section of the indicator should now be hidden."
    );
  });
});

/**
 * Tests that if the user chooses to "Stop Sharing" a display while
 * sharing their display on multiple sites, all of those display sharing
 * streams are closed.
 */
add_task(async function test_stop_sharing_multiple() {
  let indicatorPromise = promiseIndicatorWindow();

  info("Opening first tab");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing camera, microphone and screen");
  await shareDevices(tab1.linkedBrowser, true, true, true);

  info("Opening second tab");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  info("Sharing camera and screen");
  await shareDevices(tab2.linkedBrowser, true, false, true);

  let indicator = await indicatorPromise;

  let stopSharingButton = indicator.document.getElementById("stop-sharing");
  let stopSharingPromise = TestUtils.waitForCondition(() => {
    return !webrtcUI.showScreenSharingIndicator;
  });
  stopSharingButton.click();
  await stopSharingPromise;

  Assert.equal(gBrowser.selectedTab, tab2, "Should have tab2 selected.");
  await checkSharingUI({ audio: false, video: true }, window, {
    audio: true,
    video: true,
  });
  BrowserTestUtils.removeTab(tab2);

  Assert.equal(gBrowser.selectedTab, tab1, "Should have tab1 selected.");
  await checkSharingUI({ audio: true, video: true }, window, {
    audio: true,
    video: true,
  });

  // Ensure that the "display-share" section of the indicator is now hidden
  Assert.ok(
    BrowserTestUtils.is_hidden(
      indicator.document.getElementById("display-share")
    ),
    "The display-share section of the indicator should now be hidden."
  );

  BrowserTestUtils.removeTab(tab1);
});
