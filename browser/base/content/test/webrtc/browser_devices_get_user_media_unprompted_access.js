/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

var gTests = [
  {
    desc: "getUserMedia audio+camera",
    run: async function checkAudioVideoWhileLiveTracksExist_audio_camera() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
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

      // If there's an active audio+camera stream,
      // gUM(audio+camera) returns a stream without prompting;
      promise = promiseMessage("ok");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      await promiseNoPopupNotification("webRTC-shareDevices");
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");

      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await checkSharingUI({ audio: true, video: true });

      // gUM(screen) causes a prompt.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );

      // After closing all streams, gUM(audio+camera) causes a prompt.
      await closeStream();
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
      await checkNotSharing();
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );
    },
  },

  {
    desc: "getUserMedia camera",
    run: async function checkAudioVideoWhileLiveTracksExist_camera() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
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
      await checkSharingUI({ audio: false, video: true });

      // If there's an active camera stream,
      // gUM(audio) causes a prompt;
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, false);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, false);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );

      // gUM(audio+camera) causes a prompt;
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );

      // gUM(screen) causes a prompt;
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );

      // gUM(camera) returns a stream without prompting.
      promise = promiseMessage("ok");
      await promiseRequestDevice(false, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      await promiseNoPopupNotification("webRTC-shareDevices");
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");

      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await checkSharingUI({ audio: false, video: true });

      // close all streams
      await closeStream();
    },
  },

  {
    desc: "getUserMedia audio",
    run: async function checkAudioVideoWhileLiveTracksExist_audio() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, false);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      let indicator = promiseIndicatorWindow();

      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });

      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true },
        "expected microphone to be shared"
      );
      await indicator;
      await checkSharingUI({ audio: true, video: false });

      // If there's an active audio stream,
      // gUM(camera) causes a prompt;
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );

      // gUM(audio+camera) causes a prompt;
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );

      // gUM(audio) returns a stream without prompting.
      promise = promiseMessage("ok");
      await promiseRequestDevice(true, false);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      await promiseNoPopupNotification("webRTC-shareDevices");
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");

      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true },
        "expected microphone to be shared"
      );

      await checkSharingUI({ audio: true, video: false });

      // close all streams
      await closeStream();
    },
  },
];

add_task(async function test() {
  await runTests(gTests);
});
