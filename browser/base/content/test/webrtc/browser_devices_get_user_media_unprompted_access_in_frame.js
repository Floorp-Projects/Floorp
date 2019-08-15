/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

var gTests = [
  {
    desc: "getUserMedia audio+camera in frame 1",
    run: async function checkAudioVideoWhileLiveTracksExist_frame() {
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

      info("gUM(audio+camera) in frame 2 should prompt");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame2");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
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

      // If there's an active audio+camera stream in frame 1,
      // gUM(audio+camera) in frame 1 returns a stream without prompting;
      promise = promiseMessage("ok");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      await promiseNoPopupNotification("webRTC-shareDevices");
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");

      // close the stream
      await closeStream(false, "frame1");
    },
  },

  {
    desc: "getUserMedia audio+camera in frame 1 - part II",
    run: async function checkAudioVideoWhileLiveTracksExist_frame_partII() {
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

      // If there's an active audio+camera stream in frame 1,
      // gUM(audio+camera) in the top level window causes a prompt;
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

      // close the stream
      await closeStream(false, "frame1");
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
    desc: "getUserMedia audio+camera in frame 1 - reload",
    run: async function checkAudioVideoWhileLiveTracksExist_frame_reload() {
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

      // reload frame 1
      promise = promiseObserverCalled("recording-device-stopped");
      await promiseReloadFrame("frame1");
      await promise;

      await checkNotSharing();
      await expectObserverCalled("recording-device-events");
      await expectObserverCalled("recording-window-ended");
      await expectNoObserverCalled();

      // After the reload,
      // gUM(audio+camera) in frame 1 causes a prompt.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
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
    desc: "getUserMedia audio+camera at the top level window",
    run: async function checkAudioVideoWhileLiveTracksExist_topLevel() {
      // create an active audio+camera stream at the top level window
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
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
      await checkSharingUI({ audio: true, video: true });

      // If there's an active audio+camera stream at the top level window,
      // gUM(audio+camera) in frame 2 causes a prompt.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame2");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");

      // close the stream
      await closeStream();
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
];

add_task(async function test() {
  await runTests(gTests, { relativeURI: "get_user_media_in_frame.html" });
});
