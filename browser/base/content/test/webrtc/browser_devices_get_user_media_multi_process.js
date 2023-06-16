/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [
  {
    desc: "getUserMedia audio in a first process + video in a second process",
    // These tests call enableObserverVerification manually on a second tab, so
    // don't add listeners to the first tab.
    skipObserverVerification: true,
    run: async function checkMultiProcess() {
      // The main purpose of this test is to ensure webrtc sharing indicators
      // work with multiple content processes, but it makes sense to run this
      // test without e10s too to ensure using webrtc devices in two different
      // tabs is handled correctly.

      // Request audio in the first tab.
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true);
      await promise;
      await observerPromise;

      checkDeviceSelectors(["microphone"]);

      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true },
        "expected microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ audio: true });

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        !webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator hidden"
      );
      ok(
        webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator shown"
      );
      is(
        webrtcUI.getActiveStreams(false, true).length,
        1,
        "1 active audio stream"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        1,
        "1 active stream"
      );

      // If we have reached the max process count already, increase it to ensure
      // our new tab can have its own content process.
      let childCount = Services.ppmm.childCount;
      let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
      // The first check is because if we are on a branch where e10s-multi is
      // disabled, we want to keep testing e10s with a single content process.
      // The + 1 is because ppmm.childCount also counts the chrome process
      // (which also runs process scripts).
      if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
        await SpecialPowers.pushPrefEnv({
          set: [["dom.ipc.processCount", childCount]],
        });
      }

      // Open a new tab with a different hostname.
      let url = gBrowser.currentURI.spec.replace(
        "https://example.com/",
        "http://127.0.0.1:8888/"
      );
      let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

      await enableObserverVerification();

      // Request video.
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;

      checkDeviceSelectors(["camera"]);

      observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
      observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;

      await checkSharingUI({ video: true }, window, {
        audio: true,
        video: true,
      });

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator shown"
      );
      ok(
        webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator shown"
      );
      is(
        webrtcUI.getActiveStreams(false, true).length,
        1,
        "1 active audio stream"
      );
      is(webrtcUI.getActiveStreams(true).length, 1, "1 active video stream");
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        2,
        "2 active streams"
      );

      info("removing the second tab");

      await disableObserverVerification();

      BrowserTestUtils.removeTab(tab);

      // Check that we still show the sharing indicators for the first tab's stream.
      await Promise.all([
        TestUtils.waitForCondition(() => !webrtcUI.showCameraIndicator),
        TestUtils.waitForCondition(
          () => webrtcUI.getActiveStreams(true, true, true).length == 1
        ),
      ]);

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        !webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator hidden"
      );
      ok(
        webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator shown"
      );
      is(
        webrtcUI.getActiveStreams(false, true).length,
        1,
        "1 active audio stream"
      );

      await checkSharingUI({ audio: true });

      // Close the first tab's stream and verify that all indicators are removed.
      await closeStream(false, null, true);

      ok(
        !webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator hidden"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        0,
        "0 active streams"
      );
    },
  },

  {
    desc: "getUserMedia camera in a first process + camera in a second process",
    skipObserverVerification: true,
    run: async function checkMultiProcessCamera() {
      let observerPromise = expectObserverCalled("getUserMedia:request");
      // Request camera in the first tab.
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;

      checkDeviceSelectors(["camera"]);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true });

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator shown"
      );
      ok(
        !webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator hidden"
      );
      is(webrtcUI.getActiveStreams(true).length, 1, "1 active camera stream");
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        1,
        "1 active stream"
      );

      // If we have reached the max process count already, increase it to ensure
      // our new tab can have its own content process.
      let childCount = Services.ppmm.childCount;
      let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
      // The first check is because if we are on a branch where e10s-multi is
      // disabled, we want to keep testing e10s with a single content process.
      // The + 1 is because ppmm.childCount also counts the chrome process
      // (which also runs process scripts).
      if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
        await SpecialPowers.pushPrefEnv({
          set: [["dom.ipc.processCount", childCount]],
        });
      }

      // Open a new tab with a different hostname.
      let url = gBrowser.currentURI.spec.replace(
        "https://example.com/",
        "http://127.0.0.1:8888/"
      );
      let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

      await enableObserverVerification();

      // Request camera in the second tab.
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;

      checkDeviceSelectors(["camera"]);

      observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
      observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;

      await checkSharingUI({ video: true }, window, { video: true });

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator shown"
      );
      ok(
        !webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator hidden"
      );
      is(webrtcUI.getActiveStreams(true).length, 2, "2 active camera streams");
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        2,
        "2 active streams"
      );

      await disableObserverVerification();

      info("removing the second tab");
      BrowserTestUtils.removeTab(tab);

      // Check that we still show the sharing indicators for the first tab's stream.
      await Promise.all([
        TestUtils.waitForCondition(() => webrtcUI.showCameraIndicator),
        TestUtils.waitForCondition(
          () => webrtcUI.getActiveStreams(true).length == 1
        ),
      ]);
      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator shown"
      );
      ok(
        !webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator hidden"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        1,
        "1 active stream"
      );

      await checkSharingUI({ video: true });

      // Close the first tab's stream and verify that all indicators are removed.
      await closeStream(false, null, true);

      ok(
        !webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator hidden"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        0,
        "0 active streams"
      );
    },
  },

  {
    desc: "getUserMedia screen sharing in a first process + screen sharing in a second process",
    skipObserverVerification: true,
    run: async function checkMultiProcessScreen() {
      // Request screen sharing in the first tab.
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(["screen"]);

      // Select the last screen so that we can have a stream.
      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { screen: "Screen" },
        "expected screen to be shared"
      );

      await indicator;
      await checkSharingUI({ screen: "Screen" });

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        webrtcUI.showScreenSharingIndicator,
        "webrtcUI wants the screen sharing indicator shown"
      );
      ok(
        !webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator hidden"
      );
      ok(
        !webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator hidden"
      );
      is(
        webrtcUI.getActiveStreams(false, false, true).length,
        1,
        "1 active screen sharing stream"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        1,
        "1 active stream"
      );

      // If we have reached the max process count already, increase it to ensure
      // our new tab can have its own content process.
      let childCount = Services.ppmm.childCount;
      let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
      // The first check is because if we are on a branch where e10s-multi is
      // disabled, we want to keep testing e10s with a single content process.
      // The + 1 is because ppmm.childCount also counts the chrome process
      // (which also runs process scripts).
      if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
        await SpecialPowers.pushPrefEnv({
          set: [["dom.ipc.processCount", childCount]],
        });
      }

      // Open a new tab with a different hostname.
      let url = gBrowser.currentURI.spec.replace(
        "https://example.com/",
        "https://example.com/"
      );
      let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

      await enableObserverVerification();

      // Request screen sharing in the second tab.
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(["screen"]);

      // Select the last screen so that we can have a stream.
      menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();

      observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
      observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;

      await checkSharingUI({ screen: "Screen" }, window, { screen: "Screen" });

      ok(
        webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator shown"
      );
      ok(
        webrtcUI.showScreenSharingIndicator,
        "webrtcUI wants the screen sharing indicator shown"
      );
      ok(
        !webrtcUI.showCameraIndicator,
        "webrtcUI wants the camera indicator hidden"
      );
      ok(
        !webrtcUI.showMicrophoneIndicator,
        "webrtcUI wants the mic indicator hidden"
      );
      is(
        webrtcUI.getActiveStreams(false, false, true).length,
        2,
        "2 active desktop sharing streams"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        2,
        "2 active streams"
      );

      await disableObserverVerification();

      info("removing the second tab");
      BrowserTestUtils.removeTab(tab);

      await TestUtils.waitForCondition(
        () => webrtcUI.getActiveStreams(true, true, true).length == 1
      );

      // Close the first tab's stream and verify that all indicators are removed.
      await closeStream(false, null, true);

      ok(
        !webrtcUI.showGlobalIndicator,
        "webrtcUI wants the global indicator hidden"
      );
      is(
        webrtcUI.getActiveStreams(true, true, true).length,
        0,
        "0 active streams"
      );
    },
  },
];

add_task(async function test() {
  await runTests(gTests);
});
