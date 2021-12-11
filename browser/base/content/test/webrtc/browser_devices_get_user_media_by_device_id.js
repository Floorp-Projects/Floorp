/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Utility function that should be called after a request for a device
 * has been made. This function will allow sharing that device, and then
 * immediately close the stream.
 */
async function allowStreamsThenClose() {
  let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
  let observerPromise2 = expectObserverCalled("recording-device-events");
  await promiseMessage("ok", () => {
    PopupNotifications.panel.firstElementChild.button.click();
  });
  await observerPromise1;
  await observerPromise2;

  await closeStream();
}

/**
 * Tests that if a site requests a particular device by ID, that
 * the Permission Panel menulist for that device shows only that
 * device and is disabled.
 */
add_task(async function test_get_user_media_by_device_id() {
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  let devices = await navigator.mediaDevices.enumerateDevices();
  let audioId = devices
    .filter(d => d.kind == "audioinput")
    .map(d => d.deviceId)[0];
  let videoId = devices
    .filter(d => d.kind == "videoinput")
    .map(d => d.deviceId)[0];

  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    let observerPromise = expectObserverCalled("getUserMedia:request");
    await promiseRequestDevice({ deviceId: { exact: audioId } });
    await promise;
    await observerPromise;
    checkDeviceSelectors(["microphone"]);

    await allowStreamsThenClose();

    promise = promisePopupNotificationShown("webRTC-shareDevices");
    observerPromise = expectObserverCalled("getUserMedia:request");
    await promiseRequestDevice(false, { deviceId: { exact: videoId } });
    await promise;
    await observerPromise;
    checkDeviceSelectors(["camera"]);

    await allowStreamsThenClose();

    promise = promisePopupNotificationShown("webRTC-shareDevices");
    observerPromise = expectObserverCalled("getUserMedia:request");
    await promiseRequestDevice(
      { deviceId: { exact: audioId } },
      { deviceId: { exact: videoId } }
    );
    await promise;
    await observerPromise;
    checkDeviceSelectors(["microphone", "camera"]);
    await allowStreamsThenClose();
  });
});
