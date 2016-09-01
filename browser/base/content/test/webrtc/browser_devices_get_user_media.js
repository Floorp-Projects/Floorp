/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

const permissionError = "error: NotAllowedError: The request is not allowed " +
    "by the user agent or the platform in the current context.";

var gTests = [

{
  desc: "getUserMedia audio+video",
  run: function* checkAudioVideo() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
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
    is((yield getMediaCaptureState()), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({audio: true, video: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio only",
  run: function* checkAudioOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareMicrophone-notification-icon", "anchored to mic icon");
    checkDeviceSelectors(true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareMicrophone", "panel using microphone icon");

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    is((yield getMediaCaptureState()), "Microphone",
       "expected microphone to be shared");

    yield indicator;
    yield checkSharingUI({audio: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia video only",
  run: function* checkVideoOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareDevices-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareDevices", "panel using devices icon");

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    is((yield getMediaCaptureState()), "Camera", "expected camera to be shared");

    yield indicator;
    yield checkSharingUI({video: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user clicks \"Don't Share\"",
  run: function* checkDontShare() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
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
  }
},

{
  desc: "getUserMedia audio+video: stop sharing",
  run: function* checkStopSharing() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    is((yield getMediaCaptureState()), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    yield stopSharing();

    // the stream is already closed, but this will do some cleanup anyway
    yield closeStream(true);
  }
},

{
  desc: "getUserMedia audio+video: reloading the page removes all gUM UI",
  run: function* checkReloading() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    is((yield getMediaCaptureState()), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    info("reloading the web page");
    promise = promiseObserverCalled("recording-device-events");
    content.location.reload();
    yield promise;

    if ((yield promiseTodoObserverNotCalled("recording-device-events")) == 1) {
      todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
    }

    yield expectObserverCalled("recording-window-ended");
    yield expectNoObserverCalled();
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia prompt: Always/Never Share",
  run: function* checkRememberCheckbox() {
    let elt = id => document.getElementById(id);

    function* checkPerm(aRequestAudio, aRequestVideo,
                       aExpectedAudioPerm, aExpectedVideoPerm, aNever) {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      yield promiseRequestDevice(aRequestAudio, aRequestVideo);
      yield promise;
      yield expectObserverCalled("getUserMedia:request");

      is(elt("webRTC-selectMicrophone").hidden, !aRequestAudio,
         "microphone selector expected to be " + (aRequestAudio ? "visible" : "hidden"));

      is(elt("webRTC-selectCamera").hidden, !aRequestVideo,
         "camera selector expected to be " + (aRequestVideo ? "visible" : "hidden"));

      let expectedMessage = aNever ? permissionError : "ok";
      yield promiseMessage(expectedMessage, () => {
        activateSecondaryAction(aNever ? kActionNever : kActionAlways);
      });
      let expected = [];
      if (expectedMessage == "ok") {
        yield expectObserverCalled("getUserMedia:response:allow");
        yield expectObserverCalled("recording-device-events");
        if (aRequestVideo)
          expected.push("Camera");
        if (aRequestAudio)
          expected.push("Microphone");
        expected = expected.join("And");
      }
      else {
        yield expectObserverCalled("getUserMedia:response:deny");
        yield expectObserverCalled("recording-window-ended");
        expected = "none";
      }
      is((yield getMediaCaptureState()), expected,
         "expected " + expected + " to be shared");

      function checkDevicePermissions(aDevice, aExpected) {
        let Perms = Services.perms;
        let uri = gBrowser.selectedBrowser.documentURI;
        let devicePerms = Perms.testExactPermission(uri, aDevice);
        if (aExpected === undefined)
          is(devicePerms, Perms.UNKNOWN_ACTION, "no " + aDevice + " persistent permissions");
        else {
          is(devicePerms, aExpected ? Perms.ALLOW_ACTION : Perms.DENY_ACTION,
             aDevice + " persistently " + (aExpected ? "allowed" : "denied"));
        }
        Perms.remove(uri, aDevice);
      }
      checkDevicePermissions("microphone", aExpectedAudioPerm);
      checkDevicePermissions("camera", aExpectedVideoPerm);

      if (expectedMessage == "ok")
        yield closeStream();
    }

    // 3 cases where the user accepts the device prompt.
    info("audio+video, user grants, expect both perms set to allow");
    yield checkPerm(true, true, true, true);
    info("audio only, user grants, check audio perm set to allow, video perm not set");
    yield checkPerm(true, false, true, undefined);
    info("video only, user grants, check video perm set to allow, audio perm not set");
    yield checkPerm(false, true, undefined, true);

    // 3 cases where the user rejects the device request by using 'Never Share'.
    info("audio only, user denies, expect audio perm set to deny, video not set");
    yield checkPerm(true, false, false, undefined, true);
    info("video only, user denies, expect video perm set to deny, audio perm not set");
    yield checkPerm(false, true, undefined, false, true);
    info("audio+video, user denies, expect both perms set to deny");
    yield checkPerm(true, true, false, false, true);
  }
},

{
  desc: "getUserMedia without prompt: use persistent permissions",
  run: function* checkUsePersistentPermissions() {
    function* usePerm(aAllowAudio, aAllowVideo, aRequestAudio, aRequestVideo,
                     aExpectStream) {
      let Perms = Services.perms;
      let uri = gBrowser.selectedBrowser.documentURI;

      if (aAllowAudio !== undefined) {
        Perms.add(uri, "microphone", aAllowAudio ? Perms.ALLOW_ACTION
                                                 : Perms.DENY_ACTION);
      }
      if (aAllowVideo !== undefined) {
        Perms.add(uri, "camera", aAllowVideo ? Perms.ALLOW_ACTION
                                             : Perms.DENY_ACTION);
      }

      if (aExpectStream === undefined) {
        // Check that we get a prompt.
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        yield promiseRequestDevice(aRequestAudio, aRequestVideo);
        yield promise;
        yield expectObserverCalled("getUserMedia:request");

        // Deny the request to cleanup...
        yield promiseMessage(permissionError, () => {
          activateSecondaryAction(kActionDeny);
        });
        yield expectObserverCalled("getUserMedia:response:deny");
        yield expectObserverCalled("recording-window-ended");
      }
      else {
        let expectedMessage = aExpectStream ? "ok" : permissionError;
        let promise = promiseMessage(expectedMessage);
        yield promiseRequestDevice(aRequestAudio, aRequestVideo);
        yield promise;

        if (expectedMessage == "ok") {
          yield expectObserverCalled("getUserMedia:request");
          yield promiseNoPopupNotification("webRTC-shareDevices");
          yield expectObserverCalled("getUserMedia:response:allow");
          yield expectObserverCalled("recording-device-events");

          // Check what's actually shared.
          let expected = [];
          if (aAllowVideo && aRequestVideo)
            expected.push("Camera");
          if (aAllowAudio && aRequestAudio)
            expected.push("Microphone");
          expected = expected.join("And");
          is((yield getMediaCaptureState()), expected,
             "expected " + expected + " to be shared");

          yield closeStream();
        }
        else {
          yield expectObserverCalled("recording-window-ended");
        }
      }

      Perms.remove(uri, "camera");
      Perms.remove(uri, "microphone");
    }

    // Set both permissions identically
    info("allow audio+video, request audio+video, expect ok (audio+video)");
    yield usePerm(true, true, true, true, true);
    info("deny audio+video, request audio+video, expect denied");
    yield usePerm(false, false, true, true, false);

    // Allow audio, deny video.
    info("allow audio, deny video, request audio+video, expect denied");
    yield usePerm(true, false, true, true, false);
    info("allow audio, deny video, request audio, expect ok (audio)");
    yield usePerm(true, false, true, false, true);
    info("allow audio, deny video, request video, expect denied");
    yield usePerm(true, false, false, true, false);

    // Deny audio, allow video.
    info("deny audio, allow video, request audio+video, expect denied");
    yield usePerm(false, true, true, true, false);
    info("deny audio, allow video, request audio, expect denied");
    yield usePerm(false, true, true, false, false);
    info("deny audio, allow video, request video, expect ok (video)");
    yield usePerm(false, true, false, true, true);

    // Allow audio, video not set.
    info("allow audio, request audio+video, expect prompt");
    yield usePerm(true, undefined, true, true, undefined);
    info("allow audio, request audio, expect ok (audio)");
    yield usePerm(true, undefined, true, false, true);
    info("allow audio, request video, expect prompt");
    yield usePerm(true, undefined, false, true, undefined);

    // Deny audio, video not set.
    info("deny audio, request audio+video, expect denied");
    yield usePerm(false, undefined, true, true, false);
    info("deny audio, request audio, expect denied");
    yield usePerm(false, undefined, true, false, false);
    info("deny audio, request video, expect prompt");
    yield usePerm(false, undefined, false, true, undefined);

    // Allow video, audio not set.
    info("allow video, request audio+video, expect prompt");
    yield usePerm(undefined, true, true, true, undefined);
    info("allow video, request audio, expect prompt");
    yield usePerm(undefined, true, true, false, undefined);
    info("allow video, request video, expect ok (video)");
    yield usePerm(undefined, true, false, true, true);

    // Deny video, audio not set.
    info("deny video, request audio+video, expect denied");
    yield usePerm(undefined, false, true, true, false);
    info("deny video, request audio, expect prompt");
    yield usePerm(undefined, false, true, false, undefined);
    info("deny video, request video, expect denied");
    yield usePerm(undefined, false, false, true, false);
  }
},

{
  desc: "Stop Sharing removes persistent permissions",
  run: function* checkStopSharingRemovesPersistentPermissions() {
    function* stopAndCheckPerm(aRequestAudio, aRequestVideo) {
      let Perms = Services.perms;
      let uri = gBrowser.selectedBrowser.documentURI;

      // Initially set both permissions to 'allow'.
      Perms.add(uri, "microphone", Perms.ALLOW_ACTION);
      Perms.add(uri, "camera", Perms.ALLOW_ACTION);

      let indicator = promiseIndicatorWindow();
      // Start sharing what's been requested.
      let promise = promiseMessage("ok");
      yield promiseRequestDevice(aRequestAudio, aRequestVideo);
      yield promise;

      yield expectObserverCalled("getUserMedia:request");
      yield expectObserverCalled("getUserMedia:response:allow");
      yield expectObserverCalled("recording-device-events");
      yield indicator;
      yield checkSharingUI({video: aRequestVideo, audio: aRequestAudio});

      yield stopSharing(aRequestVideo ? "camera" : "microphone");

      // Check that permissions have been removed as expected.
      let audioPerm = Perms.testExactPermission(uri, "microphone");
      if (aRequestAudio)
        is(audioPerm, Perms.UNKNOWN_ACTION, "microphone permissions removed");
      else
        is(audioPerm, Perms.ALLOW_ACTION, "microphone permissions untouched");

      let videoPerm = Perms.testExactPermission(uri, "camera");
      if (aRequestVideo)
        is(videoPerm, Perms.UNKNOWN_ACTION, "camera permissions removed");
      else
        is(videoPerm, Perms.ALLOW_ACTION, "camera permissions untouched");

      // Cleanup.
      yield closeStream(true);

      Perms.remove(uri, "camera");
      Perms.remove(uri, "microphone");
    }

    info("request audio+video, stop sharing resets both");
    yield stopAndCheckPerm(true, true);
    info("request audio, stop sharing resets audio only");
    yield stopAndCheckPerm(true, false);
    info("request video, stop sharing resets video only");
    yield stopAndCheckPerm(false, true);
  }
},

{
  desc: "test showControlCenter",
  run: function* checkShowControlCenter() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    is((yield getMediaCaptureState()), "Camera", "expected camera to be shared");

    yield indicator;
    yield checkSharingUI({video: true});

    ok(gIdentityHandler._identityPopup.hidden, "control center should be hidden");
    if ("nsISystemStatusBar" in Ci) {
      let activeStreams = webrtcUI.getActiveStreams(true, false, false);
      webrtcUI.showSharingDoorhanger(activeStreams[0], "Devices");
    }
    else {
      let win =
        Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
      let elt = win.document.getElementById("audioVideoButton");
      EventUtils.synthesizeMouseAtCenter(elt, {}, win);
      yield promiseWaitForCondition(() => !gIdentityHandler._identityPopup.hidden);
    }
    ok(!gIdentityHandler._identityPopup.hidden, "control center should be open");

    gIdentityHandler._identityPopup.hidden = true;
    yield expectNoObserverCalled();

    yield closeStream();
  }
},

{
  desc: "'Always Allow' ignored and not shown on http pages",
  run: function* checkNoAlwaysOnHttp() {
    // Load an http page instead of the https version.
    let browser = gBrowser.selectedBrowser;
    browser.loadURI(browser.documentURI.spec.replace("https://", "http://"));
    yield BrowserTestUtils.browserLoaded(browser);

    // Initially set both permissions to 'allow'.
    let Perms = Services.perms;
    let uri = browser.documentURI;
    Perms.add(uri, "microphone", Perms.ALLOW_ACTION);
    Perms.add(uri, "camera", Perms.ALLOW_ACTION);

    // Request devices and expect a prompt despite the saved 'Allow' permission,
    // because the connection isn't secure.
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    // Ensure that the 'Always Allow' action isn't shown.
    let alwaysLabel = gNavigatorBundle.getString("getUserMedia.always.label");
    ok(!!alwaysLabel, "found the 'Always Allow' localized label");
    let labels = [];
    let notification = PopupNotifications.panel.firstChild;
    for (let node of notification.childNodes) {
      if (node.localName == "menuitem")
        labels.push(node.getAttribute("label"));
    }
    is(labels.indexOf(alwaysLabel), -1, "The 'Always Allow' item isn't shown");

    // Cleanup.
    yield closeStream(true);
    Perms.remove(uri, "camera");
    Perms.remove(uri, "microphone");
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
    ok(gIdentityHandler._identityPopup.hidden,
       "should start the test with the control center hidden");

    Task.spawn(function* () {
      yield SpecialPowers.pushPrefEnv({"set": [[PREF_PERMISSION_FAKE, true]]});

      for (let test of gTests) {
        info(test.desc);
        yield test.run();

        // Cleanup before the next test
        yield expectNoObserverCalled();
      }
    }).then(finish, ex => {
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
  }, true);
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "https://example.com/");
  content.location = rootDir + "get_user_media.html";
}
