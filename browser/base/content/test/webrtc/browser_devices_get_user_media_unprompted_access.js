/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

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
    yield closeStream(false, 0, 2);
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
    yield closeStream(false, 0, 2);
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
    yield closeStream(false, 0, 2);
  }
}

];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  browser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  browser.addEventListener("load", function() {
    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");
    ok(gIdentityHandler._identityPopup.hidden,
       "should start the test with the control center hidden");

    Task.spawn(function* () {
      yield SpecialPowers.pushPrefEnv({"set": [[PREF_PERMISSION_FAKE, true]]});

      for (let testCase of gTests) {
        info(testCase.desc);
        yield testCase.run();

        // Cleanup before the next test
        yield expectNoObserverCalled();
      }
    }).then(finish, ex => {
     Cu.reportError(ex);
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
  }, {capture: true, once: true});
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "https://example.com/");
  content.location = rootDir + "get_user_media.html";
}
