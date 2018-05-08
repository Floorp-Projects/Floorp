/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const permissionError = "error: NotAllowedError: The request is not allowed " +
    "by the user agent or the platform in the current context.";

const CAMERA_PREF = "permissions.default.camera";
const MICROPHONE_PREF = "permissions.default.microphone";

var gTests = [

{
  desc: "getUserMedia audio+video: globally blocking camera",
  run: async function checkAudioVideo() {
    Services.prefs.setIntPref(CAMERA_PREF, SitePermissions.BLOCK);

    // Requesting audio+video shouldn't work.
    let promise = promiseMessage(permissionError);
    await promiseRequestDevice(true, true);
    await promise;
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();

    // Requesting only video shouldn't work.
    promise = promiseMessage(permissionError);
    await promiseRequestDevice(false, true);
    await promise;
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();

    // Requesting audio should work.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(true);
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareMicrophone-notification-icon", "anchored to mic icon");
    checkDeviceSelectors(true);
    let iconclass =
      PopupNotifications.panel.firstChild.getAttribute("iconclass");
    ok(iconclass.includes("microphone-icon"), "panel using microphone icon");

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {audio: true},
                     "expected microphone to be shared");

    await indicator;
    await checkSharingUI({audio: true});
    await closeStream();
    Services.prefs.clearUserPref(CAMERA_PREF);
  }
},

{
  desc: "getUserMedia video: globally blocking camera + local exception",
  run: async function checkAudioVideo() {
    let browser = gBrowser.selectedBrowser;
    Services.prefs.setIntPref(CAMERA_PREF, SitePermissions.BLOCK);
    // Overwrite the permission for that URI, requesting video should work again.
    SitePermissions.set(browser.currentURI, "camera", SitePermissions.ALLOW);

    // Requesting video should work.
    let indicator = promiseIndicatorWindow();
    let promise = promiseMessage("ok");
    await promiseRequestDevice(false, true);
    await promise;

    await expectObserverCalled("getUserMedia:request");
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    await indicator;
    await checkSharingUI({video: true});
    await closeStream();

    SitePermissions.remove(browser.currentURI, "camera");
    Services.prefs.clearUserPref(CAMERA_PREF);
  }
},

{
  desc: "getUserMedia audio+video: globally blocking microphone",
  run: async function checkAudioVideo() {
    Services.prefs.setIntPref(MICROPHONE_PREF, SitePermissions.BLOCK);

    // Requesting audio+video shouldn't work.
    let promise = promiseMessage(permissionError);
    await promiseRequestDevice(true, true);
    await promise;
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();

    // Requesting only audio shouldn't work.
    promise = promiseMessage(permissionError);
    await promiseRequestDevice(true);
    await promise;
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();

    // Requesting video should work.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true);
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareDevices-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, true);
    let iconclass =
      PopupNotifications.panel.firstChild.getAttribute("iconclass");
    ok(iconclass.includes("camera-icon"), "panel using devices icon");

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {video: true},
                     "expected camera to be shared");

    await indicator;
    await checkSharingUI({video: true});
    await closeStream();
    Services.prefs.clearUserPref(MICROPHONE_PREF);
  }
},

{
  desc: "getUserMedia audio: globally blocking microphone + local exception",
  run: async function checkAudioVideo() {
    let browser = gBrowser.selectedBrowser;
    Services.prefs.setIntPref(MICROPHONE_PREF, SitePermissions.BLOCK);
    // Overwrite the permission for that URI, requesting video should work again.
    SitePermissions.set(browser.currentURI, "microphone", SitePermissions.ALLOW);

    // Requesting audio should work.
    let indicator = promiseIndicatorWindow();
    let promise = promiseMessage("ok");
    await promiseRequestDevice(true);
    await promise;

    await expectObserverCalled("getUserMedia:request");
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    await indicator;
    await checkSharingUI({audio: true});
    await closeStream();

    SitePermissions.remove(browser.currentURI, "microphone");
    Services.prefs.clearUserPref(MICROPHONE_PREF);
  }
},

];
add_task(async function test() {
  await runTests(gTests);
});
