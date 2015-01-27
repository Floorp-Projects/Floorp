/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kObservedTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended"
];

const PREF_PERMISSION_FAKE = "media.navigator.permission.fake";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

var gObservedTopics = {};
function observer(aSubject, aTopic, aData) {
  if (!(aTopic in gObservedTopics))
    gObservedTopics[aTopic] = 1;
  else
    ++gObservedTopics[aTopic];
}

function promiseObserverCalled(aTopic, aAction) {
  let deferred = Promise.defer();

  Services.obs.addObserver(function observer() {
    ok(true, "got " + aTopic + " notification");
    Services.obs.removeObserver(observer, aTopic);

    if (kObservedTopics.indexOf(aTopic) != -1) {
      if (!(aTopic in gObservedTopics))
        gObservedTopics[aTopic] = -1;
      else
        --gObservedTopics[aTopic];
    }

    deferred.resolve();
  }, aTopic, false);

  if (aAction)
    aAction();

  return deferred.promise;
}

function expectObserverCalled(aTopic) {
  is(gObservedTopics[aTopic], 1, "expected notification " + aTopic);
  if (aTopic in gObservedTopics)
    --gObservedTopics[aTopic];
}

function expectNoObserverCalled() {
  for (let topic in gObservedTopics) {
    if (gObservedTopics[topic])
      is(gObservedTopics[topic], 0, topic + " notification unexpected");
  }
  gObservedTopics = {};
}

function promiseMessage(aMessage, aAction) {
  let deferred = Promise.defer();

  content.addEventListener("message", function messageListener(event) {
    content.removeEventListener("message", messageListener);
    is(event.data, aMessage, "received " + aMessage);
    if (event.data == aMessage)
      deferred.resolve();
    else
      deferred.reject();
  });

  if (aAction)
    aAction();

  return deferred.promise;
}

function promisePopupNotificationShown(aName, aAction) {
  let deferred = Promise.defer();

  PopupNotifications.panel.addEventListener("popupshown", function popupNotifShown() {
    PopupNotifications.panel.removeEventListener("popupshown", popupNotifShown);

    ok(!!PopupNotifications.getNotification(aName), aName + " notification shown");
    ok(PopupNotifications.isPanelOpen, "notification panel open");
    ok(!!PopupNotifications.panel.firstChild, "notification panel populated");

    deferred.resolve();
  });

  if (aAction)
    aAction();

  return deferred.promise;
}

function promisePopupNotification(aName) {
  let deferred = Promise.defer();

  waitForCondition(() => PopupNotifications.getNotification(aName),
                   () => {
    ok(!!PopupNotifications.getNotification(aName),
       aName + " notification appeared");

    deferred.resolve();
  }, "timeout waiting for popup notification " + aName);

  return deferred.promise;
}

function promiseNoPopupNotification(aName) {
  let deferred = Promise.defer();

  waitForCondition(() => !PopupNotifications.getNotification(aName),
                   () => {
    ok(!PopupNotifications.getNotification(aName),
       aName + " notification removed");
    deferred.resolve();
  }, "timeout waiting for popup notification " + aName + " to disappear");

  return deferred.promise;
}

function enableDevice(aType, aEnabled) {
  let menulist = document.getElementById("webRTC-select" + aType + "-menulist");
  let menupopup = document.getElementById("webRTC-select" + aType + "-menupopup");
  menulist.value = aEnabled ? menupopup.firstChild.getAttribute("value") : "-1";
}

const kActionAlways = 1;
const kActionDeny = 2;
const kActionNever = 3;

function activateSecondaryAction(aAction) {
  let notification = PopupNotifications.panel.firstChild;
  notification.button.focus();
  let popup = notification.menupopup;
  popup.addEventListener("popupshown", function () {
    popup.removeEventListener("popupshown", arguments.callee, false);

    // Press 'down' as many time as needed to select the requested action.
    while (aAction--)
      EventUtils.synthesizeKey("VK_DOWN", {});

    // Activate
    EventUtils.synthesizeKey("VK_RETURN", {});
  }, false);

  // One down event to open the popup
  EventUtils.synthesizeKey("VK_DOWN",
                           { altKey: !navigator.platform.contains("Mac") });
}

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
  kObservedTopics.forEach(topic => {
    Services.obs.removeObserver(observer, topic);
  });
  Services.prefs.clearUserPref(PREF_PERMISSION_FAKE);
});

function getMediaCaptureState() {
  let hasVideo = {};
  let hasAudio = {};
  MediaManagerService.mediaCaptureWindowState(content, hasVideo, hasAudio);
  if (hasVideo.value && hasAudio.value)
    return "CameraAndMicrophone";
  if (hasVideo.value)
    return "Camera";
  if (hasAudio.value)
    return "Microphone";
  return "none";
}

function* closeStream(aAlreadyClosed) {
  expectNoObserverCalled();

  info("closing the stream");
  content.wrappedJSObject.closeStream();

  if (!aAlreadyClosed)
    yield promiseObserverCalled("recording-device-events");

  yield promiseNoPopupNotification("webRTC-sharingDevices");
  if (!aAlreadyClosed)
    expectObserverCalled("recording-window-ended");

  yield* assertWebRTCIndicatorStatus(null);
}

function checkDeviceSelectors(aAudio, aVideo) {
  let micSelector = document.getElementById("webRTC-selectMicrophone");
  if (aAudio)
    ok(!micSelector.hidden, "microphone selector visible");
  else
    ok(micSelector.hidden, "microphone selector hidden");

  let cameraSelector = document.getElementById("webRTC-selectCamera");
  if (aVideo)
    ok(!cameraSelector.hidden, "camera selector visible");
  else
    ok(cameraSelector.hidden, "camera selector hidden");
}

function* checkSharingUI(aExpected) {
  yield promisePopupNotification("webRTC-sharingDevices");

  yield* assertWebRTCIndicatorStatus(aExpected);
}

function* checkNotSharing() {
  is(getMediaCaptureState(), "none", "expected nothing to be shared");

  ok(!PopupNotifications.getNotification("webRTC-sharingDevices"),
     "no webRTC-sharingDevices popup notification");

  yield* assertWebRTCIndicatorStatus(null);
}

const permissionError = "error: PermissionDeniedError: The user did not grant permission for the operation.";

let gTests = [

{
  desc: "getUserMedia audio+video",
  run: function checkAudioVideo() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareDevices-notification-icon", "anchored to device icon");
    checkDeviceSelectors(true, true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareDevices", "panel using devices icon");

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield checkSharingUI({audio: true, video: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio only",
  run: function checkAudioOnly() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true);
    });
    expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareMicrophone-notification-icon", "anchored to mic icon");
    checkDeviceSelectors(true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareMicrophone", "panel using microphone icon");

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "Microphone", "expected microphone to be shared");

    yield checkSharingUI({audio: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia video only",
  run: function checkVideoOnly() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(false, true);
    });
    expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareDevices-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareDevices", "panel using devices icon");

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "Camera", "expected camera to be shared");

    yield checkSharingUI({video: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables video",
  run: function checkDisableVideo() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    // disable the camera
    enableDevice("Camera", false);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitem to have no impact on the following tests.
    enableDevice("Camera", true);

    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "Microphone",
       "expected microphone to be shared");

    yield checkSharingUI({audio: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables audio",
  run: function checkDisableAudio() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    // disable the microphone
    enableDevice("Microphone", false);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitem to have no impact on the following tests.
    enableDevice("Microphone", true);

    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "Camera",
       "expected microphone to be shared");

    yield checkSharingUI({video: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables both audio and video",
  run: function checkDisableAudioVideo() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    // disable the camera and microphone
    enableDevice("Camera", false);
    enableDevice("Microphone", false);

    yield promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitems to have no impact on the following tests.
    enableDevice("Camera", true);
    enableDevice("Microphone", true);

    expectObserverCalled("getUserMedia:response:deny");
    expectObserverCalled("recording-window-ended");
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video, user clicks \"Don't Share\"",
  run: function checkDontShare() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    expectObserverCalled("getUserMedia:response:deny");
    expectObserverCalled("recording-window-ended");
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video: stop sharing",
  run: function checkStopSharing() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield checkSharingUI({video: true, audio: true});

    yield promiseNotificationShown(PopupNotifications.getNotification("webRTC-sharingDevices"));
    activateSecondaryAction(kActionDeny);

    yield promiseObserverCalled("recording-device-events");
    expectObserverCalled("getUserMedia:revoke");

    yield promiseNoPopupNotification("webRTC-sharingDevices");
    expectObserverCalled("recording-window-ended");

    if (gObservedTopics["recording-device-events"] == 1) {
      todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
      gObservedTopics["recording-device-events"] = 0;
    }

    expectNoObserverCalled();
    yield checkNotSharing();

    // the stream is already closed, but this will do some cleanup anyway
    yield closeStream(true);
  }
},

{
  desc: "getUserMedia audio+video: reloading the page removes all gUM UI",
  run: function checkReloading() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield checkSharingUI({video: true, audio: true});

    yield promiseNotificationShown(PopupNotifications.getNotification("webRTC-sharingDevices"));

    info("reloading the web page");
    yield promiseObserverCalled("recording-device-events",
                                () => { content.location.reload(); });

    yield promiseNoPopupNotification("webRTC-sharingDevices");
    if (gObservedTopics["recording-device-events"] == 1) {
      todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
      gObservedTopics["recording-device-events"] = 0;
    }
    expectObserverCalled("recording-window-ended");
    expectNoObserverCalled();
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia prompt: Always/Never Share",
  run: function checkRememberCheckbox() {
    let elt = id => document.getElementById(id);

    function checkPerm(aRequestAudio, aRequestVideo, aAllowAudio, aAllowVideo,
                       aExpectedAudioPerm, aExpectedVideoPerm, aNever) {
      yield promisePopupNotificationShown("webRTC-shareDevices", () => {
        content.wrappedJSObject.requestDevice(aRequestAudio, aRequestVideo);
      });
      expectObserverCalled("getUserMedia:request");

      let noAudio = aAllowAudio === undefined;
      is(elt("webRTC-selectMicrophone").hidden, noAudio,
         "microphone selector expected to be " + (noAudio ? "hidden" : "visible"));
      if (!noAudio)
        enableDevice("Microphone", aAllowAudio || aNever);

      let noVideo = aAllowVideo === undefined;
      is(elt("webRTC-selectCamera").hidden, noVideo,
         "camera selector expected to be " + (noVideo ? "hidden" : "visible"));
      if (!noVideo)
        enableDevice("Camera", aAllowVideo || aNever);

      let expectedMessage =
        (aAllowVideo || aAllowAudio) ? "ok" : permissionError;
      yield promiseMessage(expectedMessage, () => {
        activateSecondaryAction(aNever ? kActionNever : kActionAlways);
      });
      let expected = [];
      if (expectedMessage == "ok") {
        expectObserverCalled("getUserMedia:response:allow");
        expectObserverCalled("recording-device-events");
        if (aAllowVideo)
          expected.push("Camera");
        if (aAllowAudio)
          expected.push("Microphone");
        expected = expected.join("And");
      }
      else {
        expectObserverCalled("getUserMedia:response:deny");
        expectObserverCalled("recording-window-ended");
        expected = "none";
      }
      is(getMediaCaptureState(), expected,
         "expected " + expected + " to be shared");

      function checkDevicePermissions(aDevice, aExpected) {
        let Perms = Services.perms;
        let uri = content.document.documentURIObject;
        let devicePerms = Perms.testExactPermission(uri, aDevice);
        if (aExpected === undefined)
          is(devicePerms, Perms.UNKNOWN_ACTION, "no " + aDevice + " persistent permissions");
        else {
          is(devicePerms, aExpected ? Perms.ALLOW_ACTION : Perms.DENY_ACTION,
             aDevice + " persistently " + (aExpected ? "allowed" : "denied"));
        }
        Perms.remove(uri.host, aDevice);
      }
      checkDevicePermissions("microphone", aExpectedAudioPerm);
      checkDevicePermissions("camera", aExpectedVideoPerm);

      if (expectedMessage == "ok")
        yield closeStream();
    }

    // 3 cases where the user accepts the device prompt.
    info("audio+video, user grants, expect both perms set to allow");
    yield checkPerm(true, true, true, true, true, true);
    info("audio only, user grants, check audio perm set to allow, video perm not set");
    yield checkPerm(true, false, true, undefined, true, undefined);
    info("video only, user grants, check video perm set to allow, audio perm not set");
    yield checkPerm(false, true, undefined, true, undefined, true);

    // 3 cases where the user rejects the device request.
    // First test these cases by setting the device to 'No Audio'/'No Video'
    info("audio+video, user denies, expect both perms set to deny");
    yield checkPerm(true, true, false, false, false, false);
    info("audio only, user denies, expect audio perm set to deny, video not set");
    yield checkPerm(true, false, false, undefined, false, undefined);
    info("video only, user denies, expect video perm set to deny, audio perm not set");
    yield checkPerm(false, true, undefined, false, undefined, false);
    // Now test these 3 cases again by using the 'Never Share' action.
    info("audio+video, user denies, expect both perms set to deny");
    yield checkPerm(true, true, false, false, false, false, true);
    info("audio only, user denies, expect audio perm set to deny, video not set");
    yield checkPerm(true, false, false, undefined, false, undefined, true);
    info("video only, user denies, expect video perm set to deny, audio perm not set");
    yield checkPerm(false, true, undefined, false, undefined, false, true);

    // 2 cases where the user allows half of what's requested.
    info("audio+video, user denies video, grants audio, " +
         "expect video perm set to deny, audio perm set to allow.");
    yield checkPerm(true, true, true, false, true, false);
    info("audio+video, user denies audio, grants video, " +
         "expect video perm set to allow, audio perm set to deny.");
    yield checkPerm(true, true, false, true, false, true);

    // reset the menuitems to have no impact on the following tests.
    enableDevice("Microphone", true);
    enableDevice("Camera", true);
  }
},

{
  desc: "getUserMedia without prompt: use persistent permissions",
  run: function checkUsePersistentPermissions() {
    function usePerm(aAllowAudio, aAllowVideo, aRequestAudio, aRequestVideo,
                     aExpectStream) {
      let Perms = Services.perms;
      let uri = content.document.documentURIObject;
      if (aAllowAudio !== undefined) {
        Perms.add(uri, "microphone", aAllowAudio ? Perms.ALLOW_ACTION
                                                 : Perms.DENY_ACTION);
      }
      if (aAllowVideo !== undefined) {
        Perms.add(uri, "camera", aAllowVideo ? Perms.ALLOW_ACTION
                                             : Perms.DENY_ACTION);
      }

      let gum = function() {
        content.wrappedJSObject.requestDevice(aRequestAudio, aRequestVideo);
      };

      if (aExpectStream === undefined) {
        // Check that we get a prompt.
        yield promisePopupNotificationShown("webRTC-shareDevices", gum);
        expectObserverCalled("getUserMedia:request");

        // Deny the request to cleanup...
        yield promiseMessage(permissionError, () => {
          activateSecondaryAction(kActionDeny);
        });
        expectObserverCalled("getUserMedia:response:deny");
        expectObserverCalled("recording-window-ended");
      }
      else {
        let expectedMessage = aExpectStream ? "ok" : permissionError;
        yield promiseMessage(expectedMessage, gum);

        if (expectedMessage == "ok") {
          expectObserverCalled("getUserMedia:request");
          yield promiseNoPopupNotification("webRTC-shareDevices");
          expectObserverCalled("getUserMedia:response:allow");
          expectObserverCalled("recording-device-events");

          // Check what's actually shared.
          let expected = [];
          if (aAllowVideo && aRequestVideo)
            expected.push("Camera");
          if (aAllowAudio && aRequestAudio)
            expected.push("Microphone");
          expected = expected.join("And");
          is(getMediaCaptureState(), expected,
             "expected " + expected + " to be shared");

          yield closeStream();
        }
        else {
          expectObserverCalled("recording-window-ended");
        }
      }

      Perms.remove(uri.host, "camera");
      Perms.remove(uri.host, "microphone");
    }

    // Set both permissions identically
    info("allow audio+video, request audio+video, expect ok (audio+video)");
    yield usePerm(true, true, true, true, true);
    info("deny audio+video, request audio+video, expect denied");
    yield usePerm(false, false, true, true, false);

    // Allow audio, deny video.
    info("allow audio, deny video, request audio+video, expect ok (audio)");
    yield usePerm(true, false, true, true, true);
    info("allow audio, deny video, request audio, expect ok (audio)");
    yield usePerm(true, false, true, false, true);
    info("allow audio, deny video, request video, expect denied");
    yield usePerm(true, false, false, true, false);

    // Deny audio, allow video.
    info("deny audio, allow video, request audio+video, expect ok (video)");
    yield usePerm(false, true, true, true, true);
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
    info("deny audio, request audio+video, expect prompt");
    yield usePerm(false, undefined, true, true, undefined);
    info("deny audio, request audio, expect denied");
    yield usePerm(false, undefined, true, false, false);
    info("deny audio, request video, expect prompt");
    yield usePerm(false, undefined, false, true, undefined);

    // Allow video, video not set.
    info("allow video, request audio+video, expect prompt");
    yield usePerm(undefined, true, true, true, undefined);
    info("allow video, request audio, expect prompt");
    yield usePerm(undefined, true, true, false, undefined);
    info("allow video, request video, expect ok (video)");
    yield usePerm(undefined, true, false, true, true);

    // Deny video, video not set.
    info("deny video, request audio+video, expect prompt");
    yield usePerm(undefined, false, true, true, undefined);
    info("deny video, request audio, expect prompt");
    yield usePerm(undefined, false, true, false, undefined);
    info("deny video, request video, expect denied");
    yield usePerm(undefined, false, false, true, false);
  }
},

{
  desc: "Stop Sharing removes persistent permissions",
  run: function checkStopSharingRemovesPersistentPermissions() {
    function stopAndCheckPerm(aRequestAudio, aRequestVideo) {
      let Perms = Services.perms;
      let uri = content.document.documentURIObject;

      // Initially set both permissions to 'allow'.
      Perms.add(uri, "microphone", Perms.ALLOW_ACTION);
      Perms.add(uri, "camera", Perms.ALLOW_ACTION);

      // Start sharing what's been requested.
      yield promiseMessage("ok", () => {
        content.wrappedJSObject.requestDevice(aRequestAudio, aRequestVideo);
      });
      expectObserverCalled("getUserMedia:request");
      expectObserverCalled("getUserMedia:response:allow");
      expectObserverCalled("recording-device-events");
      yield checkSharingUI({video: aRequestVideo, audio: aRequestAudio});

      yield promiseNotificationShown(PopupNotifications.getNotification("webRTC-sharingDevices"));
      let expectedIcon = "webRTC-sharingDevices";
      if (aRequestAudio && !aRequestVideo)
        expectedIcon = "webRTC-sharingMicrophone";
      is(PopupNotifications.getNotification("webRTC-sharingDevices").anchorID,
         expectedIcon + "-notification-icon", "anchored to correct icon");
      is(PopupNotifications.panel.firstChild.getAttribute("popupid"), expectedIcon,
         "panel using correct icon");

      // Stop sharing.
      activateSecondaryAction(kActionDeny);

      yield promiseObserverCalled("recording-device-events");
      expectObserverCalled("getUserMedia:revoke");

      yield promiseNoPopupNotification("webRTC-sharingDevices");
      expectObserverCalled("recording-window-ended");

      if (gObservedTopics["recording-device-events"] == 1) {
        todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
        gObservedTopics["recording-device-events"] = 0;
      }

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

      Perms.remove(uri.host, "camera");
      Perms.remove(uri.host, "microphone");
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
  desc: "test showSharingDoorhanger",
  run: function checkShowSharingDoorhanger() {
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(false, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "Camera", "expected camera to be shared");

    yield checkSharingUI({video: true});

    yield promisePopupNotificationShown("webRTC-sharingDevices", () => {
      if ("nsISystemStatusBar" in Ci) {
        let activeStreams = gWebRTCUI.getActiveStreams(true, false, false);
        gWebRTCUI.showSharingDoorhanger(activeStreams[0], "Devices");
      }
      else {
        let win =
          Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
        let elt = win.document.getElementById("audioVideoButton");
        EventUtils.synthesizeMouseAtCenter(elt, {}, win);
      }
    });

    PopupNotifications.panel.firstChild.button.click();
    ok(!PopupNotifications.isPanelOpen, "notification panel closed");
    expectNoObserverCalled();

    yield closeStream();
  }
},

{
  desc: "'Always Allow' ignored and not shown on http pages",
  run: function checkNoAlwaysOnHttp() {
    // Load an http page instead of the https version.
    let deferred = Promise.defer();
    let browser = gBrowser.selectedBrowser;
    browser.addEventListener("load", function onload() {
      browser.removeEventListener("load", onload, true);
      deferred.resolve();
    }, true);
    content.location = content.location.href.replace("https://", "http://");
    yield deferred.promise;

    // Initially set both permissions to 'allow'.
    let Perms = Services.perms;
    let uri = content.document.documentURIObject;
    Perms.add(uri, "microphone", Perms.ALLOW_ACTION);
    Perms.add(uri, "camera", Perms.ALLOW_ACTION);

    // Request devices and expect a prompt despite the saved 'Allow' permission,
    // because the connection isn't secure.
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      content.wrappedJSObject.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");

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
    Perms.remove(uri.host, "camera");
    Perms.remove(uri.host, "microphone");
  }
}

];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", function onload() {
    tab.linkedBrowser.removeEventListener("load", onload, true);

    kObservedTopics.forEach(topic => {
      Services.obs.addObserver(observer, topic, false);
    });
    Services.prefs.setBoolPref(PREF_PERMISSION_FAKE, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");

    Task.spawn(function () {
      for (let test of gTests) {
        info(test.desc);
        yield test.run();

        // Cleanup before the next test
        expectNoObserverCalled();
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


function wait(time) {
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, time);
  return deferred.promise;
}
