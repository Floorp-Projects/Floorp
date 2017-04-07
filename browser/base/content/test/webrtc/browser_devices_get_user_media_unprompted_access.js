/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const permissionError = "error: NotAllowedError: The request is not allowed " +
    "by the user agent or the platform in the current context.";

var gTests = [

{
  desc: "getUserMedia audio+camera",
  run: function* checkAudioVideoWhileLiveTracksExist_audio_camera() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    let indicator = promiseIndicatorWindow();

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");
    yield indicator;
    yield checkSharingUI({audio: true, video: true});

    // If there's an active audio+camera stream,
    // gUM(audio+camera) returns a stream without prompting;
    promise = promiseMessage("ok");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    yield promiseNoPopupNotification("webRTC-shareDevices");
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");

    Assert.deepEqual((yield getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");

    yield checkSharingUI({audio: true, video: true});

    // gUM(screen) causes a prompt.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);

    yield promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);

    // After closing all streams, gUM(audio+camera) causes a prompt.
    yield closeStream();
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");
    yield checkNotSharing();
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);
  }
},

{
  desc: "getUserMedia camera",
  run: function* checkAudioVideoWhileLiveTracksExist_camera() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    let indicator = promiseIndicatorWindow();

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {video: true},
                     "expected camera to be shared");
    yield indicator;
    yield checkSharingUI({audio: false, video: true});

    // If there's an active camera stream,
    // gUM(audio) causes a prompt;
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, false);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, false);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);

    // gUM(audio+camera) causes a prompt;
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);

    // gUM(screen) causes a prompt;
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);

    yield promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);

    // gUM(camera) returns a stream without prompting.
    promise = promiseMessage("ok");
    yield promiseRequestDevice(false, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    yield promiseNoPopupNotification("webRTC-shareDevices");
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");

    Assert.deepEqual((yield getMediaCaptureState()), {video: true},
                     "expected camera to be shared");

    yield checkSharingUI({audio: false, video: true});

    // close all streams
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio",
  run: function* checkAudioVideoWhileLiveTracksExist_audio() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, false);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    let indicator = promiseIndicatorWindow();

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true},
                     "expected microphone to be shared");
    yield indicator;
    yield checkSharingUI({audio: true, video: false});

    // If there's an active audio stream,
    // gUM(camera) causes a prompt;
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, true);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);

    // gUM(audio+camera) causes a prompt;
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "microphone", gBrowser.selectedBrowser);

    // gUM(audio) returns a stream without prompting.
    promise = promiseMessage("ok");
    yield promiseRequestDevice(true, false);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    yield promiseNoPopupNotification("webRTC-shareDevices");
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");

    Assert.deepEqual((yield getMediaCaptureState()), {audio: true},
                     "expected microphone to be shared");

    yield checkSharingUI({audio: true, video: false});

    // close all streams
    yield closeStream();
  }
}

];

add_task(async function test() {
  await runTests(gTests);
});
