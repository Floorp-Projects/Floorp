/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

const CAMERA_PREF = "permissions.default.camera";
const MICROPHONE_PREF = "permissions.default.microphone";

var gTests = [
  {
    desc: "getUserMedia audio+video: globally blocking camera",
    run: async function checkAudioVideo() {
      Services.prefs.setIntPref(CAMERA_PREF, SitePermissions.BLOCK);

      // Requesting audio+video shouldn't work.
      let observerPromise = expectObserverCalled("recording-window-ended");
      let promise = promiseMessage(permissionError);
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      await checkNotSharing();

      // Requesting only video shouldn't work.
      observerPromise = expectObserverCalled("recording-window-ended");
      promise = promiseMessage(permissionError);
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;
      await checkNotSharing();

      // Requesting audio should work.
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true);
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareMicrophone-notification-icon",
        "anchored to mic icon"
      );
      checkDeviceSelectors(true);

      // With Proton enabled, the icon does not appear in the panel.
      if (!gProton) {
        let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
          "iconclass"
        );
        ok(
          iconclass.includes("microphone-icon"),
          "panel using microphone icon"
        );
      }

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
      await checkSharingUI({ audio: true });
      await closeStream();
      Services.prefs.clearUserPref(CAMERA_PREF);
    },
  },

  {
    desc: "getUserMedia video: globally blocking camera + local exception",
    run: async function checkAudioVideo() {
      let browser = gBrowser.selectedBrowser;
      Services.prefs.setIntPref(CAMERA_PREF, SitePermissions.BLOCK);
      // Overwrite the permission for that URI, requesting video should work again.
      PermissionTestUtils.add(
        browser.currentURI,
        "camera",
        Services.perms.ALLOW_ACTION
      );

      // Requesting video should work.
      let indicator = promiseIndicatorWindow();
      let promises = [
        expectObserverCalled("getUserMedia:request"),
        expectObserverCalled("getUserMedia:response:allow"),
        expectObserverCalled("recording-device-events"),
      ];

      let promise = promiseMessage("ok");
      await promiseRequestDevice(false, true);
      await promise;

      await Promise.all(promises);
      await indicator;
      await checkSharingUI({ video: true }, undefined, undefined, {
        video: { scope: SitePermissions.SCOPE_PERSISTENT },
      });
      await closeStream();

      PermissionTestUtils.remove(browser.currentURI, "camera");
      Services.prefs.clearUserPref(CAMERA_PREF);
    },
  },

  {
    desc: "getUserMedia audio+video: globally blocking microphone",
    run: async function checkAudioVideo() {
      Services.prefs.setIntPref(MICROPHONE_PREF, SitePermissions.BLOCK);

      // Requesting audio+video shouldn't work.
      let observerPromise = expectObserverCalled("recording-window-ended");
      let promise = promiseMessage(permissionError);
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      await checkNotSharing();

      // Requesting only audio shouldn't work.
      observerPromise = expectObserverCalled("recording-window-ended");
      promise = promiseMessage(permissionError);
      await promiseRequestDevice(true);
      await promise;
      await observerPromise;
      await checkNotSharing();

      // Requesting video should work.
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, true);

      // With Proton enabled, the icon does not appear in the panel.
      if (!gProton) {
        let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
          "iconclass"
        );
        ok(iconclass.includes("camera-icon"), "panel using devices icon");
      }

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
      await closeStream();
      Services.prefs.clearUserPref(MICROPHONE_PREF);
    },
  },

  {
    desc: "getUserMedia audio: globally blocking microphone + local exception",
    run: async function checkAudioVideo() {
      let browser = gBrowser.selectedBrowser;
      Services.prefs.setIntPref(MICROPHONE_PREF, SitePermissions.BLOCK);
      // Overwrite the permission for that URI, requesting video should work again.
      PermissionTestUtils.add(
        browser.currentURI,
        "microphone",
        Services.perms.ALLOW_ACTION
      );

      // Requesting audio should work.
      let indicator = promiseIndicatorWindow();
      let promises = [
        expectObserverCalled("getUserMedia:request"),
        expectObserverCalled("getUserMedia:response:allow"),
        expectObserverCalled("recording-device-events"),
      ];
      let promise = promiseMessage("ok");
      await promiseRequestDevice(true);
      await promise;

      await Promise.all(promises);
      await indicator;
      await checkSharingUI({ audio: true }, undefined, undefined, {
        audio: { scope: SitePermissions.SCOPE_PERSISTENT },
      });
      await closeStream();

      PermissionTestUtils.remove(browser.currentURI, "microphone");
      Services.prefs.clearUserPref(MICROPHONE_PREF);
    },
  },
];
add_task(async function test() {
  await runTests(gTests);
});
