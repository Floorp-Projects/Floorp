/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

function promiseReloadFrame(aFrameId) {
  return ContentTask.spawn(gBrowser.selectedBrowser, aFrameId, function*(contentFrameId) {
    content.wrappedJSObject
           .document
           .getElementById(contentFrameId)
           .contentWindow
           .location
           .reload();
  });
}

var gTests = [

{
  desc: "getUserMedia audio+video",
  run: function* checkAudioVideo() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, "frame1");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareDevices-notification-icon", "anchored to device icon");
    checkDeviceSelectors(true, true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareDevices", "panel using devices icon");

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
    yield closeStream(false, "frame1");
  }
},

{
  desc: "getUserMedia audio+video: stop sharing",
  run: function* checkStopSharing() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, "frame1");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      activateSecondaryAction(kActionAlways);
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    let Perms = Services.perms;
    let uri = Services.io.newURI("https://example.com/", null, null);
    is(Perms.testExactPermission(uri, "microphone"), Perms.ALLOW_ACTION,
                                 "microphone persistently allowed");
    is(Perms.testExactPermission(uri, "camera"), Perms.ALLOW_ACTION,
                                 "camera persistently allowed");

    yield stopSharing();

    // The persistent permissions for the frame should have been removed.
    is(Perms.testExactPermission(uri, "microphone"), Perms.UNKNOWN_ACTION,
                                 "microphone not persistently allowed");
    is(Perms.testExactPermission(uri, "camera"), Perms.UNKNOWN_ACTION,
                                 "camera not persistently allowed");

    // the stream is already closed, but this will do some cleanup anyway
    yield closeStream(true, "frame1");
  }
},

{
  desc: "getUserMedia audio+video: reloading the frame removes all sharing UI",
  run: function* checkReloading() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, "frame1");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    info("reloading the frame");
    promise = promiseObserverCalled("recording-device-events");
    yield promiseReloadFrame("frame1");
    yield promise;

    yield expectObserverCalled("recording-window-ended");
    yield expectNoObserverCalled();
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video: reloading the frame removes prompts",
  run: function* checkReloadingRemovesPrompts() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, "frame1");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    info("reloading the frame");
    promise = promiseObserverCalled("recording-window-ended");
    yield promiseReloadFrame("frame1");
    yield promise;
    yield promiseNoPopupNotification("webRTC-shareDevices");

    yield expectNoObserverCalled();
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video: reloading a frame updates the sharing UI",
  run: function* checkUpdateWhenReloading() {
    // We'll share only the mic in the first frame, then share both in the
    // second frame, then reload the second frame. After each step, we'll check
    // the UI is in the correct state.

    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, false, "frame1");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, false);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true},
                     "expected microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: false, audio: true});
    yield expectNoObserverCalled();

    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, "frame2");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");

    yield checkSharingUI({video: true, audio: true});
    yield expectNoObserverCalled();

    info("reloading the second frame");
    promise = promiseObserverCalled("recording-device-events");
    yield promiseReloadFrame("frame2");
    yield promise;

    yield expectObserverCalled("recording-window-ended");
    yield checkSharingUI({video: false, audio: true});
    yield expectNoObserverCalled();

    yield closeStream(false, "frame1");
    yield expectNoObserverCalled();
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video: reloading the top level page removes all sharing UI",
  run: function* checkReloading() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, "frame1");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    yield reloadAndAssertClosedStreams();
  }
}

];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  browser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");

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
  }, true);
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "https://example.com/");
  let url = rootDir + "get_user_media.html";
  content.location = 'data:text/html,<iframe id="frame1" src="' + url + '"></iframe><iframe id="frame2" src="' + url + '"></iframe>'
}
