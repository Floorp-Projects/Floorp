/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [
  {
    desc: "getUserMedia audio+video",
    run: async function checkAudioVideo() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(true, true);
      is(
        PopupNotifications.panel.firstElementChild.getAttribute("popupid"),
        "webRTC-shareDevices",
        "panel using devices icon"
      );

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ audio: true, video: true });
      await closeStream(false, "frame1");
    },
  },

  {
    desc: "getUserMedia audio+video: stop sharing",
    run: async function checkStopSharing() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        activateSecondaryAction(kActionAlways);
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      let Perms = Services.perms;
      let uri = Services.io.newURI("https://example.com/");
      is(
        Perms.testExactPermission(uri, "microphone"),
        Perms.ALLOW_ACTION,
        "microphone persistently allowed"
      );
      is(
        Perms.testExactPermission(uri, "camera"),
        Perms.ALLOW_ACTION,
        "camera persistently allowed"
      );

      await stopSharing();

      // The persistent permissions for the frame should have been removed.
      is(
        Perms.testExactPermission(uri, "microphone"),
        Perms.UNKNOWN_ACTION,
        "microphone not persistently allowed"
      );
      is(
        Perms.testExactPermission(uri, "camera"),
        Perms.UNKNOWN_ACTION,
        "camera not persistently allowed"
      );

      // the stream is already closed, but this will do some cleanup anyway
      await closeStream(true, "frame1");
    },
  },

  {
    desc:
      "getUserMedia audio+video: reloading the frame removes all sharing UI",
    run: async function checkReloading() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      info("reloading the frame");
      let promises = [
        promiseObserverCalled("recording-device-stopped"),
        promiseObserverCalled("recording-device-events"),
        promiseObserverCalled("recording-window-ended"),
      ];
      await promiseReloadFrame("frame1");
      await Promise.all(promises);

      await expectNoObserverCalled();
      await checkNotSharing();
    },
  },

  {
    desc: "getUserMedia audio+video: reloading the frame removes prompts",
    run: async function checkReloadingRemovesPrompts() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      info("reloading the frame");
      promise = promiseObserverCalled("recording-window-ended");
      await promiseReloadFrame("frame1");
      await promise;
      await promiseNoPopupNotification("webRTC-shareDevices");

      await expectNoObserverCalled();
      await checkNotSharing();
    },
  },

  {
    desc:
      "getUserMedia audio+video: with two frames sharing at the same time, sharing UI shows all shared devices",
    run: async function checkFrameOverridingSharingUI() {
      // This tests an edge case discovered in bug 1440356 that works like this
      // - Share audio and video in iframe 1.
      // - Share only video in iframe 2.
      // The WebRTC UI should still show both video and audio indicators.

      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });
      await expectNoObserverCalled();

      // Check that requesting a new device from a different frame
      // doesn't override sharing UI.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, "frame2");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, true);

      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await checkSharingUI({ video: true, audio: true });

      // Check that ending the stream with the other frame
      // doesn't override sharing UI.
      promise = promiseObserverCalled("recording-device-events");
      await promiseReloadFrame("frame2");
      await promise;

      await expectObserverCalled("recording-window-ended");
      await checkSharingUI({ video: true, audio: true });
      await expectNoObserverCalled();

      await closeStream(false, "frame1");
      await expectNoObserverCalled();
      await checkNotSharing();
    },
  },

  {
    desc: "getUserMedia audio+video: reloading a frame updates the sharing UI",
    run: async function checkUpdateWhenReloading() {
      // We'll share only the cam in the first frame, then share both in the
      // second frame, then reload the second frame. After each step, we'll check
      // the UI is in the correct state.

      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: false });
      await expectNoObserverCalled();

      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame2");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await checkSharingUI({ video: true, audio: true });
      await expectNoObserverCalled();

      info("reloading the second frame");
      promise = promiseObserverCalled("recording-device-events");
      await promiseReloadFrame("frame2");
      await promise;

      await expectObserverCalled("recording-window-ended");
      await checkSharingUI({ video: true, audio: false });
      await expectNoObserverCalled();

      await closeStream(false, "frame1");
      await expectNoObserverCalled();
      await checkNotSharing();
    },
  },

  {
    desc:
      "getUserMedia audio+video: reloading the top level page removes all sharing UI",
    run: async function checkReloading() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      await reloadAndAssertClosedStreams();
    },
  },
];

add_task(async function test() {
  await runTests(gTests, { relativeURI: "get_user_media_in_frame.html" });
});
