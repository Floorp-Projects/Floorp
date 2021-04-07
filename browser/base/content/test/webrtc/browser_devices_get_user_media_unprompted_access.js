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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
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
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );
      await indicator;
      await checkSharingUI({ audio: true, video: true });

      // If there's an active audio+camera stream,
      // gUM(audio+camera) returns a stream without prompting;
      let observerPromises = [
        expectObserverCalled("getUserMedia:request"),
        expectObserverCalled("getUserMedia:response:allow"),
        expectObserverCalled("recording-device-events"),
      ];

      promise = promiseMessage("ok");

      await promiseRequestDevice(true, true);
      await promise;
      await Promise.all(observerPromises);

      await promiseNoPopupNotification("webRTC-shareDevices");

      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await checkSharingUI({ audio: true, video: true });

      // gUM(screen) causes a prompt.
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
      checkDeviceSelectors(false, false, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });
      await observerPromise;

      // Revoke screen block (only). Don't over-revoke ahead of remaining steps.
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );

      // After closing all streams, gUM(audio+camera) causes a prompt.
      await closeStream();
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
      observerPromise2 = expectObserverCalled("recording-window-ended");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise1;
      await observerPromise2;

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
    desc: "getUserMedia audio+camera -camera",
    run: async function checkAudioVideoWhileLiveTracksExist_audio_nocamera() {
      // State: fresh

      {
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        const request = expectObserverCalled("getUserMedia:request");
        await promiseRequestDevice(true, true);
        await popupShown;
        await request;
        const indicator = promiseIndicatorWindow();
        const response = expectObserverCalled("getUserMedia:response:allow");
        const deviceEvents = expectObserverCalled("recording-device-events");
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await response;
        await deviceEvents;
        Assert.deepEqual(
          await getMediaCaptureState(),
          { audio: true, video: true },
          "expected camera and microphone to be shared"
        );
        await indicator;
        await checkSharingUI({ audio: true, video: true });

        // Stop the camera track.
        await stopTracks("video");
        await checkSharingUI({ audio: true, video: false });
      }

      // State: live audio

      {
        // If there's an active audio track from an audio+camera request,
        // gUM(camera) causes a prompt.
        const request = expectObserverCalled("getUserMedia:request");
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(false, true);
        await popupShown;
        await request;
        checkDeviceSelectors(false, true);

        // Allow and stop the camera again.
        const response = expectObserverCalled("getUserMedia:response:allow");
        const deviceEvents = expectObserverCalled("recording-device-events");
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await response;
        await deviceEvents;
        Assert.deepEqual(
          await getMediaCaptureState(),
          { audio: true, video: true },
          "expected camera and microphone to be shared"
        );
        await checkSharingUI({ audio: true, video: true });

        await stopTracks("video");
        await checkSharingUI({ audio: true, video: false });
      }

      // State: live audio

      {
        // If there's an active audio track from an audio+camera request,
        // gUM(audio+camera) causes a prompt.
        const request = expectObserverCalled("getUserMedia:request");
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(true, true);
        await popupShown;
        await request;
        checkDeviceSelectors(true, true);

        // Allow and stop the camera again.
        const response = expectObserverCalled("getUserMedia:response:allow");
        const deviceEvents = expectObserverCalled("recording-device-events");
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await response;
        await deviceEvents;
        Assert.deepEqual(
          await getMediaCaptureState(),
          { audio: true, video: true },
          "expected camera and microphone to be shared"
        );
        await checkSharingUI({ audio: true, video: true });

        await stopTracks("video");
        await checkSharingUI({ audio: true, video: false });
      }

      // State: live audio

      {
        // After closing all streams, gUM(audio) causes a prompt.
        await closeStream();
        const request = expectObserverCalled("getUserMedia:request");
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(true, false);
        await popupShown;
        await request;
        checkDeviceSelectors(true, false);

        const response = expectObserverCalled("getUserMedia:response:deny");
        const windowEnded = expectObserverCalled("recording-window-ended");

        await promiseMessage(permissionError, () => {
          activateSecondaryAction(kActionDeny);
        });

        await response;
        await windowEnded;

        await checkNotSharing();
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
      }
    },
  },

  {
    desc: "getUserMedia audio+camera -audio",
    run: async function checkAudioVideoWhileLiveTracksExist_camera_noaudio() {
      // State: fresh

      {
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        const request = expectObserverCalled("getUserMedia:request");
        await promiseRequestDevice(true, true);
        await popupShown;
        await request;
        const indicator = promiseIndicatorWindow();
        const response = expectObserverCalled("getUserMedia:response:allow");
        const deviceEvents = expectObserverCalled("recording-device-events");
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await response;
        await deviceEvents;
        Assert.deepEqual(
          await getMediaCaptureState(),
          { audio: true, video: true },
          "expected camera and microphone to be shared"
        );
        await indicator;
        await checkSharingUI({ audio: true, video: true });

        // Stop the audio track.
        await stopTracks("audio");
        await checkSharingUI({ audio: false, video: true });
      }

      // State: live camera

      {
        // If there's an active video track from an audio+camera request,
        // gUM(audio) causes a prompt.
        const request = expectObserverCalled("getUserMedia:request");
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(true, false);
        await popupShown;
        await request;
        checkDeviceSelectors(true, false);

        // Allow and stop the microphone again.
        const response = expectObserverCalled("getUserMedia:response:allow");
        const deviceEvents = expectObserverCalled("recording-device-events");
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await response;
        await deviceEvents;
        Assert.deepEqual(
          await getMediaCaptureState(),
          { audio: true, video: true },
          "expected camera and microphone to be shared"
        );
        await checkSharingUI({ audio: true, video: true });

        await stopTracks("audio");
        await checkSharingUI({ audio: false, video: true });
      }

      // State: live camera

      {
        // If there's an active video track from an audio+camera request,
        // gUM(audio+camera) causes a prompt.
        const request = expectObserverCalled("getUserMedia:request");
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(true, true);
        await popupShown;
        await request;
        checkDeviceSelectors(true, true);

        // Allow and stop the microphone again.
        const response = expectObserverCalled("getUserMedia:response:allow");
        const deviceEvents = expectObserverCalled("recording-device-events");
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await response;
        await deviceEvents;
        Assert.deepEqual(
          await getMediaCaptureState(),
          { audio: true, video: true },
          "expected camera and microphone to be shared"
        );
        await checkSharingUI({ audio: true, video: true });

        await stopTracks("audio");
        await checkSharingUI({ audio: false, video: true });
      }

      // State: live camera

      {
        // After closing all streams, gUM(camera) causes a prompt.
        await closeStream();
        const request = expectObserverCalled("getUserMedia:request");
        const popupShown = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(false, true);
        await popupShown;
        await request;
        checkDeviceSelectors(false, true);

        const response = expectObserverCalled("getUserMedia:response:deny");
        const windowEnded = expectObserverCalled("recording-window-ended");

        await promiseMessage(permissionError, () => {
          activateSecondaryAction(kActionDeny);
        });

        await response;
        await windowEnded;

        await checkNotSharing();
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
      }
    },
  },

  {
    desc: "getUserMedia camera",
    run: async function checkAudioVideoWhileLiveTracksExist_camera() {
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;
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
      await checkSharingUI({ audio: false, video: true });

      // If there's an active camera stream,
      // gUM(audio) causes a prompt;
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, false);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, false);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise;
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
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise;
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
      checkDeviceSelectors(false, false, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise;
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
      let observerPromises = [
        expectObserverCalled("getUserMedia:request"),
        expectObserverCalled("getUserMedia:response:allow"),
        expectObserverCalled("recording-device-events"),
      ];

      promise = promiseMessage("ok");
      await promiseRequestDevice(false, true);
      await promise;
      await Promise.all(observerPromises);

      await promiseNoPopupNotification("webRTC-shareDevices");

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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, false);
      await promise;
      await observerPromise;
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
        { audio: true },
        "expected microphone to be shared"
      );
      await indicator;
      await checkSharingUI({ audio: true, video: false });

      // If there's an active audio stream,
      // gUM(camera) causes a prompt;
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(false, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise;
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
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise = expectObserverCalled("getUserMedia:response:deny");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise;
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
      let observerPromises = [
        expectObserverCalled("getUserMedia:request"),
        expectObserverCalled("getUserMedia:response:allow"),
        expectObserverCalled("recording-device-events"),
      ];
      promise = promiseMessage("ok");
      await promiseRequestDevice(true, false);
      await promise;
      await observerPromises;
      await promiseNoPopupNotification("webRTC-shareDevices");

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
