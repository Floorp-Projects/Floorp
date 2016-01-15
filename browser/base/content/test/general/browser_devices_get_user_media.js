/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

const PREF_PERMISSION_FAKE = "media.navigator.permission.fake";

let frameScript = function() {

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

const kObservedTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended"
];

var gObservedTopics = {};
function observer(aSubject, aTopic, aData) {
  if (!(aTopic in gObservedTopics))
    gObservedTopics[aTopic] = 1;
  else
    ++gObservedTopics[aTopic];
}

kObservedTopics.forEach(topic => {
  Services.obs.addObserver(observer, topic, false);
});

addMessageListener("Test:ExpectObserverCalled", ({data}) => {
  sendAsyncMessage("Test:ExpectObserverCalled:Reply",
                   {count: gObservedTopics[data]});
  if (data in gObservedTopics)
    --gObservedTopics[data];
});

addMessageListener("Test:TodoObserverNotCalled", ({data}) => {
  sendAsyncMessage("Test:TodoObserverNotCalled:Reply",
                   {count: gObservedTopics[data]});
  if (gObservedTopics[data] == 1)
    gObservedTopics[data] = 0;
});

addMessageListener("Test:ExpectNoObserverCalled", data => {
  sendAsyncMessage("Test:ExpectNoObserverCalled:Reply", gObservedTopics);
  gObservedTopics = {};
});

function _getMediaCaptureState() {
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

addMessageListener("Test:GetMediaCaptureState", data => {
  sendAsyncMessage("Test:MediaCaptureState", _getMediaCaptureState());
});

addMessageListener("Test:WaitForObserverCall", ({data}) => {
  let topic = data;
  Services.obs.addObserver(function observer() {
    sendAsyncMessage("Test:ObserverCalled", topic);
    Services.obs.removeObserver(observer, topic);

    if (kObservedTopics.indexOf(topic) != -1) {
      if (!(topic in gObservedTopics))
        gObservedTopics[topic] = -1;
      else
        --gObservedTopics[topic];
    }
  }, topic, false);
});

}; // end of framescript

function _mm() {
  return gBrowser.selectedBrowser.messageManager;
}

function promiseObserverCalled(aTopic) {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:ObserverCalled", function listener({data}) {
      if (data == aTopic) {
        ok(true, "got " + aTopic + " notification");
        mm.removeMessageListener("Test:ObserverCalled", listener);
        resolve();
      }
    });
    mm.sendAsyncMessage("Test:WaitForObserverCall", aTopic);
  });
}

function expectObserverCalled(aTopic) {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:ExpectObserverCalled:Reply",
                          function listener({data}) {
      is(data.count, 1, "expected notification " + aTopic);
      mm.removeMessageListener("Test:ExpectObserverCalled:Reply", listener);
      resolve();
    });
    mm.sendAsyncMessage("Test:ExpectObserverCalled", aTopic);
  });
}

function expectNoObserverCalled() {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:ExpectNoObserverCalled:Reply",
                          function listener({data}) {
      mm.removeMessageListener("Test:ExpectNoObserverCalled:Reply", listener);
      for (let topic in data) {
        if (data[topic])
          is(data[topic], 0, topic + " notification unexpected");
      }
      resolve();
    });
    mm.sendAsyncMessage("Test:ExpectNoObserverCalled");
  });
}

function promiseTodoObserverNotCalled(aTopic) {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:TodoObserverNotCalled:Reply",
                          function listener({data}) {
      mm.removeMessageListener("Test:TodoObserverNotCalled:Reply", listener);
      resolve(data.count);
    });
    mm.sendAsyncMessage("Test:TodoObserverNotCalled", aTopic);
  });
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
                           { altKey: !navigator.platform.includes("Mac") });
}

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

function getMediaCaptureState() {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:MediaCaptureState", ({data}) => {
      resolve(data);
    });
    mm.sendAsyncMessage("Test:GetMediaCaptureState");
  });
}

function promiseRequestDevice(aRequestAudio, aRequestVideo) {
  info("requesting devices");
  return ContentTask.spawn(gBrowser.selectedBrowser,
                           {aRequestAudio, aRequestVideo},
                           function*(args) {
    content.wrappedJSObject.requestDevice(args.aRequestAudio,
                                          args.aRequestVideo);
  });
}

function* closeStream(aAlreadyClosed) {
  yield expectNoObserverCalled();

  let promise;
  if (!aAlreadyClosed)
    promise = promiseObserverCalled("recording-device-events");

  info("closing the stream");
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    content.wrappedJSObject.closeStream();
  });

  if (!aAlreadyClosed)
    yield promise;

  yield promiseNoPopupNotification("webRTC-sharingDevices");
  if (!aAlreadyClosed)
    yield expectObserverCalled("recording-window-ended");

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
  is((yield getMediaCaptureState()), "none", "expected nothing to be shared");

  ok(!PopupNotifications.getNotification("webRTC-sharingDevices"),
     "no webRTC-sharingDevices popup notification");

  yield* assertWebRTCIndicatorStatus(null);
}

const permissionError = "error: SecurityError: The operation is insecure.";

var gTests = [

{
  desc: "getUserMedia audio+video",
  run: function checkAudioVideo() {
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
  run: function checkAudioOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareMicrophone-notification-icon", "anchored to mic icon");
    checkDeviceSelectors(true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareMicrophone", "panel using microphone icon");

    enableDevice("Microphone", true);

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
  run: function checkVideoOnly() {
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
  desc: "getUserMedia audio+video, user disables video",
  run: function checkDisableVideo() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    // disable the camera
    enableDevice("Microphone", true);
    enableDevice("Camera", false);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitem to have no impact on the following tests.
    enableDevice("Camera", true);

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
  desc: "getUserMedia audio+video, user disables audio",
  run: function checkDisableAudio() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    // disable the microphone
    enableDevice("Microphone", false);
    enableDevice("Camera", true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitem to have no impact on the following tests.
    enableDevice("Microphone", true);

    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    is((yield getMediaCaptureState()), "Camera",
       "expected microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables both audio and video",
  run: function checkDisableAudioVideo() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
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

    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video, user clicks \"Don't Share\"",
  run: function checkDontShare() {
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
  run: function checkStopSharing() {
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

    yield promiseNotificationShown(PopupNotifications.getNotification("webRTC-sharingDevices"));
    activateSecondaryAction(kActionDeny);

    yield promiseObserverCalled("recording-device-events");
    yield expectObserverCalled("getUserMedia:revoke");

    yield promiseNoPopupNotification("webRTC-sharingDevices");
    yield expectObserverCalled("recording-window-ended");

    if ((yield promiseTodoObserverNotCalled("recording-device-events")) == 1) {
      todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
    }

    yield expectNoObserverCalled();
    yield checkNotSharing();

    // the stream is already closed, but this will do some cleanup anyway
    yield closeStream(true);
  }
},

{
  desc: "getUserMedia audio+video: reloading the page removes all gUM UI",
  run: function checkReloading() {
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

    yield promiseNotificationShown(PopupNotifications.getNotification("webRTC-sharingDevices"));

    info("reloading the web page");
    promise = promiseObserverCalled("recording-device-events");
    content.location.reload();
    yield promise;

    yield promiseNoPopupNotification("webRTC-sharingDevices");
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
  run: function checkRememberCheckbox() {
    let elt = id => document.getElementById(id);

    function checkPerm(aRequestAudio, aRequestVideo, aAllowAudio, aAllowVideo,
                       aExpectedAudioPerm, aExpectedVideoPerm, aNever) {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      yield promiseRequestDevice(aRequestAudio, aRequestVideo);
      yield promise;
      yield expectObserverCalled("getUserMedia:request");

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
        yield expectObserverCalled("getUserMedia:response:allow");
        yield expectObserverCalled("recording-device-events");
        if (aAllowVideo)
          expected.push("Camera");
        if (aAllowAudio)
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
      yield expectObserverCalled("getUserMedia:revoke");

      yield promiseNoPopupNotification("webRTC-sharingDevices");
      yield expectObserverCalled("recording-window-ended");

      if ((yield promiseTodoObserverNotCalled("recording-device-events")) == 1) {
        todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
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
  desc: "test showSharingDoorhanger",
  run: function checkShowSharingDoorhanger() {
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
    yield expectNoObserverCalled();

    yield closeStream();
  }
},

{
  desc: "'Always Allow' ignored and not shown on http pages",
  run: function checkNoAlwaysOnHttp() {
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

  browser.messageManager.loadFrameScript("data:,(" + frameScript.toString() + ")();", true);

  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");

    Task.spawn(function () {
      yield new Promise(resolve => SpecialPowers.pushPrefEnv({
        "set": [[PREF_PERMISSION_FAKE, true]],
      }, resolve));

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


function wait(time) {
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, time);
  return deferred.promise;
}
