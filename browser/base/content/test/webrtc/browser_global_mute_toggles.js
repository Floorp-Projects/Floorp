/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";
const MUTE_TOPICS = [
  "getUserMedia:muteVideo",
  "getUserMedia:unmuteVideo",
  "getUserMedia:muteAudio",
  "getUserMedia:unmuteAudio",
];

add_setup(async function () {
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
    ["privacy.webrtc.globalMuteToggles", true],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });
});

/**
 * Returns a Promise that resolves when the content process for
 * <browser> fires the right observer notification based on the
 * value of isMuted for the camera.
 *
 * Note: Callers must ensure that they first call
 * BrowserTestUtils.startObservingTopics to monitor the mute and
 * unmute observer notifications for this to work properly.
 *
 * @param {<xul:browser>} browser - The browser running in the content process
 * to be monitored.
 * @param {Boolean} isMuted - True if the muted topic should be fired.
 * @return {Promise}
 * @resolves {undefined} When the notification fires.
 */
function waitForCameraMuteState(browser, isMuted) {
  let topic = isMuted ? "getUserMedia:muteVideo" : "getUserMedia:unmuteVideo";
  return BrowserTestUtils.contentTopicObserved(browser.browsingContext, topic);
}

/**
 * Returns a Promise that resolves when the content process for
 * <browser> fires the right observer notification based on the
 * value of isMuted for the microphone.
 *
 * Note: Callers must ensure that they first call
 * BrowserTestUtils.startObservingTopics to monitor the mute and
 * unmute observer notifications for this to work properly.
 *
 * @param {<xul:browser>} browser - The browser running in the content process
 * to be monitored.
 * @param {Boolean} isMuted - True if the muted topic should be fired.
 * @return {Promise}
 * @resolves {undefined} When the notification fires.
 */
function waitForMicrophoneMuteState(browser, isMuted) {
  let topic = isMuted ? "getUserMedia:muteAudio" : "getUserMedia:unmuteAudio";
  return BrowserTestUtils.contentTopicObserved(browser.browsingContext, topic);
}

/**
 * Tests that the global mute toggles fire the right observer
 * notifications in pre-existing content processes.
 */
add_task(async function test_notifications() {
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let indicatorPromise = promiseIndicatorWindow();

    await shareDevices(browser, true /* camera */, true /* microphone */);

    let indicator = await indicatorPromise;
    let doc = indicator.document;

    let microphoneMute = doc.getElementById("microphone-mute-toggle");
    let cameraMute = doc.getElementById("camera-mute-toggle");

    Assert.ok(
      !microphoneMute.checked,
      "Microphone toggle should not start checked."
    );
    Assert.ok(!cameraMute.checked, "Camera toggle should not start checked.");

    await BrowserTestUtils.startObservingTopics(
      browser.browsingContext,
      MUTE_TOPICS
    );

    info("Muting microphone...");
    let microphoneMuted = waitForMicrophoneMuteState(browser, true);
    microphoneMute.click();
    await microphoneMuted;
    info("Microphone successfully muted.");

    info("Muting camera...");
    let cameraMuted = waitForCameraMuteState(browser, true);
    cameraMute.click();
    await cameraMuted;
    info("Camera successfully muted.");

    Assert.ok(
      microphoneMute.checked,
      "Microphone toggle should now be checked."
    );
    Assert.ok(cameraMute.checked, "Camera toggle should now be checked.");

    info("Unmuting microphone...");
    let microphoneUnmuted = waitForMicrophoneMuteState(browser, false);
    microphoneMute.click();
    await microphoneUnmuted;
    info("Microphone successfully unmuted.");

    info("Unmuting camera...");
    let cameraUnmuted = waitForCameraMuteState(browser, false);
    cameraMute.click();
    await cameraUnmuted;
    info("Camera successfully unmuted.");

    await BrowserTestUtils.stopObservingTopics(
      browser.browsingContext,
      MUTE_TOPICS
    );
  });
});

/**
 * Tests that if sharing stops while muted, and the indicator closes,
 * then the mute state is reset.
 */
add_task(async function test_closing_indicator_resets_mute() {
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let indicatorPromise = promiseIndicatorWindow();

    await shareDevices(browser, true /* camera */, true /* microphone */);

    let indicator = await indicatorPromise;
    let doc = indicator.document;

    let microphoneMute = doc.getElementById("microphone-mute-toggle");
    let cameraMute = doc.getElementById("camera-mute-toggle");

    Assert.ok(
      !microphoneMute.checked,
      "Microphone toggle should not start checked."
    );
    Assert.ok(!cameraMute.checked, "Camera toggle should not start checked.");

    await BrowserTestUtils.startObservingTopics(
      browser.browsingContext,
      MUTE_TOPICS
    );

    info("Muting microphone...");
    let microphoneMuted = waitForMicrophoneMuteState(browser, true);
    microphoneMute.click();
    await microphoneMuted;
    info("Microphone successfully muted.");

    info("Muting camera...");
    let cameraMuted = waitForCameraMuteState(browser, true);
    cameraMute.click();
    await cameraMuted;
    info("Camera successfully muted.");

    Assert.ok(
      microphoneMute.checked,
      "Microphone toggle should now be checked."
    );
    Assert.ok(cameraMute.checked, "Camera toggle should now be checked.");

    let allUnmuted = Promise.all([
      waitForMicrophoneMuteState(browser, false),
      waitForCameraMuteState(browser, false),
    ]);

    await closeStream();
    await allUnmuted;

    await BrowserTestUtils.stopObservingTopics(
      browser.browsingContext,
      MUTE_TOPICS
    );
  });
});

/**
 * Test that if the global mute state is set, then newly created
 * content processes also have their tracks muted after sending
 * a getUserMedia request.
 */
add_task(async function test_new_processes() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_PAGE,
  });
  let browser1 = tab1.linkedBrowser;

  let indicatorPromise = promiseIndicatorWindow();

  await shareDevices(browser1, true /* camera */, true /* microphone */);

  let indicator = await indicatorPromise;
  let doc = indicator.document;

  let microphoneMute = doc.getElementById("microphone-mute-toggle");
  let cameraMute = doc.getElementById("camera-mute-toggle");

  Assert.ok(
    !microphoneMute.checked,
    "Microphone toggle should not start checked."
  );
  Assert.ok(!cameraMute.checked, "Camera toggle should not start checked.");

  await BrowserTestUtils.startObservingTopics(
    browser1.browsingContext,
    MUTE_TOPICS
  );

  info("Muting microphone...");
  let microphoneMuted = waitForMicrophoneMuteState(browser1, true);
  microphoneMute.click();
  await microphoneMuted;
  info("Microphone successfully muted.");

  info("Muting camera...");
  let cameraMuted = waitForCameraMuteState(browser1, true);
  cameraMute.click();
  await cameraMuted;
  info("Camera successfully muted.");

  // We'll make sure a new process is being launched by observing
  // for the ipc:content-created notification.
  let processLaunched = TestUtils.topicObserved("ipc:content-created");

  let tab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_PAGE,
    forceNewProcess: true,
  });
  let browser2 = tab2.linkedBrowser;

  await processLaunched;

  await BrowserTestUtils.startObservingTopics(
    browser2.browsingContext,
    MUTE_TOPICS
  );

  let microphoneMuted2 = waitForMicrophoneMuteState(browser2, true);
  let cameraMuted2 = waitForCameraMuteState(browser2, true);
  info("Sharing the microphone and camera from a new process.");
  await shareDevices(browser2, true /* camera */, true /* microphone */);
  await Promise.all([microphoneMuted2, cameraMuted2]);

  info("Unmuting microphone...");
  let microphoneUnmuted = Promise.all([
    waitForMicrophoneMuteState(browser1, false),
    waitForMicrophoneMuteState(browser2, false),
  ]);
  microphoneMute.click();
  await microphoneUnmuted;
  info("Microphone successfully unmuted.");

  info("Unmuting camera...");
  let cameraUnmuted = Promise.all([
    waitForCameraMuteState(browser1, false),
    waitForCameraMuteState(browser2, false),
  ]);
  cameraMute.click();
  await cameraUnmuted;
  info("Camera successfully unmuted.");

  await BrowserTestUtils.stopObservingTopics(
    browser1.browsingContext,
    MUTE_TOPICS
  );

  await BrowserTestUtils.stopObservingTopics(
    browser2.browsingContext,
    MUTE_TOPICS
  );

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});
