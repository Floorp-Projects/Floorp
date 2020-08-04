/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Tests that if the user chooses to "Stop Sharing" a display while
 * also sharing their microphone or camera, that only the display
 * stream is stopped.
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

  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let indicatorPromise = promiseIndicatorWindow();

    // First, choose to share the microphone and camera devices.

    let promise = promisePopupNotificationShown(
      "webRTC-shareDevices",
      null,
      window
    );

    await promiseRequestDevice(
      true /* audio */,
      true /* video */,
      null /* frameID */,
      null /* screen */,
      browser
    );
    await promise;

    checkDeviceSelectors(true /* microphone */, true /* camera */);

    let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
    let observerPromise2 = expectObserverCalled("recording-device-events");
    promise = promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });

    await observerPromise1;
    await observerPromise2;
    await promise;

    promise = promisePopupNotificationShown(
      "webRTC-shareDevices",
      null,
      window
    );

    // Next, choose to share a display stream.

    await promiseRequestDevice(
      false /* audio */,
      true /* video */,
      null /* frameID */,
      "screen" /* video type */,
      browser
    );
    await promise;

    checkDeviceSelectors(
      false /* microphone */,
      false /* camera */,
      true /* screen */,
      window
    );

    let document = window.document;

    // Select one of the windows / screens. It doesn't really matter which.
    let menulist = document.getElementById("webRTC-selectWindow-menulist");
    menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();
    let notification = window.PopupNotifications.panel.firstElementChild;

    observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
    observerPromise2 = expectObserverCalled("recording-device-events");
    await promiseMessage(
      "ok",
      () => {
        notification.button.click();
      },
      1,
      browser
    );
    await observerPromise1;
    await observerPromise2;

    let indicator = await indicatorPromise;
    let indicatorDoc = indicator.document;

    // We're choosing the last item in the display list by default, which
    // _should_ be a screen. Just in case it isn't, we check whether or not
    // the indicator is showing the "Stop Sharing" button for screens, and
    // use that button for the test. Otherwise, we use the "Stop Sharing"
    // button for other windows.
    let isSharingScreen = indicatorDoc.documentElement.hasAttribute(
      "sharingscreen"
    );
    let stopSharingID = isSharingScreen
      ? "stop-sharing-screen"
      : "stop-sharing-window";

    let stopSharingButton = indicator.document.getElementById(stopSharingID);
    let stopSharingPromise = expectObserverCalled("recording-device-events");
    stopSharingButton.click();
    await stopSharingPromise;

    // Ensure that we're still sharing the other streams.
    await checkSharingUI({ audio: true, video: true });
  });
});
