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

function promiseNotification(aTopic, aAction) {
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

function expectNotification(aTopic) {
  is(gObservedTopics[aTopic], 1, "expected notification " + aTopic);
  if (aTopic in gObservedTopics)
    --gObservedTopics[aTopic];
}

function expectNoNotifications() {
  for (let topic in gObservedTopics) {
    if (gObservedTopics[topic])
      is(gObservedTopics[topic], 0, topic + " notification unexpected");
  }
  gObservedTopics = {}
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

function closeStream(aAlreadyClosed) {
  expectNoNotifications();

  info("closing the stream");
  content.wrappedJSObject.closeStream();

  if (!aAlreadyClosed)
    yield promiseNotification("recording-device-events");

  yield promiseNoPopupNotification("webRTC-sharingDevices");
  if (!aAlreadyClosed)
    expectNotification("recording-window-ended");

  let statusButton = document.getElementById("webrtc-status-button");
  ok(statusButton.hidden, "WebRTC status button hidden");
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

function checkSharingUI() {
  yield promisePopupNotification("webRTC-sharingDevices");
  let statusButton = document.getElementById("webrtc-status-button");
  ok(!statusButton.hidden, "WebRTC status button visible");
}

function checkNotSharing() {
  is(getMediaCaptureState(), "none", "expected nothing to be shared");

  ok(!PopupNotifications.getNotification("webRTC-sharingDevices"),
     "no webRTC-sharingDevices popup notification");

  let statusButton = document.getElementById("webrtc-status-button");
  ok(statusButton.hidden, "WebRTC status button hidden");
}

let gTests = [

{
  desc: "getUserMedia audio+video",
  run: function checkAudioVideo() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectNotification("getUserMedia:response:allow");
    expectNotification("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield checkSharingUI();
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio only",
  run: function checkAudioOnly() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectNotification("getUserMedia:response:allow");
    expectNotification("recording-device-events");
    is(getMediaCaptureState(), "Microphone", "expected microphone to be shared");

    yield checkSharingUI();
    yield closeStream();
  }
},

{
  desc: "getUserMedia video only",
  run: function checkVideoOnly() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(false, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(false, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectNotification("getUserMedia:response:allow");
    expectNotification("recording-device-events");
    is(getMediaCaptureState(), "Camera", "expected camera to be shared");

    yield checkSharingUI();
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables video",
  run: function checkDisableVideo() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true, true);

    // disable the camera
    document.getElementById("webRTC-selectCamera-menulist").value = -1;

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitem to have no impact on the following tests.
    document.getElementById("webRTC-selectCamera-menulist").value = 0;

    expectNotification("getUserMedia:response:allow");
    expectNotification("recording-device-events");
    is(getMediaCaptureState(), "Microphone",
       "expected microphone to be shared");

    yield checkSharingUI();
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables audio",
  run: function checkDisableAudio() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true, true);

    // disable the microphone
    document.getElementById("webRTC-selectMicrophone-menulist").value = -1;

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitem to have no impact on the following tests.
    document.getElementById("webRTC-selectMicrophone-menulist").value = 0;

    expectNotification("getUserMedia:response:allow");
    expectNotification("recording-device-events");
    is(getMediaCaptureState(), "Camera",
       "expected microphone to be shared");

    yield checkSharingUI();
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+video, user disables both audio and video",
  run: function checkDisableAudioVideo() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true, true);

    // disable the camera and microphone
    document.getElementById("webRTC-selectCamera-menulist").value = -1;
    document.getElementById("webRTC-selectMicrophone-menulist").value = -1;

    yield promiseMessage("error: PERMISSION_DENIED", () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    // reset the menuitems to have no impact on the following tests.
    document.getElementById("webRTC-selectCamera-menulist").value = 0;
    document.getElementById("webRTC-selectMicrophone-menulist").value = 0;

    expectNotification("getUserMedia:response:deny");
    expectNotification("recording-window-ended");
    checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video, user clicks \"Don't Share\"",
  run: function checkDontShare() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true, true);

    yield promiseMessage("error: PERMISSION_DENIED", () => {
      activateSecondaryAction(kActionDeny);
    });

    expectNotification("getUserMedia:response:deny");
    expectNotification("recording-window-ended");
    checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video: stop sharing",
  run: function checkStopSharing() {
    yield promiseNotification("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    yield promisePopupNotification("webRTC-shareDevices");
    checkDeviceSelectors(true, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectNotification("getUserMedia:response:allow");
    expectNotification("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield checkSharingUI();

    PopupNotifications.getNotification("webRTC-sharingDevices").reshow();
    activateSecondaryAction(kActionDeny);

    yield promiseNotification("recording-device-events");
    expectNotification("getUserMedia:revoke");

    yield promiseNoPopupNotification("webRTC-sharingDevices");

    if (gObservedTopics["recording-device-events"] == 1) {
      todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
      gObservedTopics["recording-device-events"] = 0;
    }

    expectNoNotifications();
    checkNotSharing();

    // the stream is already closed, but this will do some cleanup anyway
    yield closeStream(true);
  }
},

{
  desc: "getUserMedia prompt: Always/Never Share",
  run: function checkRememberCheckbox() {
    function checkPerm(aRequestAudio, aRequestVideo, aAllowAudio, aAllowVideo,
                       aExpectedAudioPerm, aExpectedVideoPerm, aNever) {
      yield promiseNotification("getUserMedia:request", () => {
        content.wrappedJSObject.requestDevice(aRequestAudio, aRequestVideo);
      });

      yield promisePopupNotification("webRTC-shareDevices");

      let elt = id => document.getElementById(id);

      let noAudio = aAllowAudio === undefined;
      is(elt("webRTC-selectMicrophone").hidden, noAudio,
         "microphone selector expected to be " + (noAudio ? "hidden" : "visible"));
      if (!noAudio)
        elt("webRTC-selectMicrophone-menulist").value = (aAllowAudio || aNever) ? 0 : -1;

      let noVideo = aAllowVideo === undefined;
      is(elt("webRTC-selectCamera").hidden, noVideo,
         "camera selector expected to be " + (noVideo ? "hidden" : "visible"));
      if (!noVideo)
        elt("webRTC-selectCamera-menulist").value = (aAllowVideo || aNever) ? 0 : -1;

      let expectedMessage =
        (aAllowVideo || aAllowAudio) ? "ok" : "error: PERMISSION_DENIED";
      yield promiseMessage(expectedMessage, () => {
        activateSecondaryAction(aNever ? kActionNever : kActionAlways);
      });
      let expected = [];
      if (expectedMessage == "ok") {
        expectNotification("getUserMedia:response:allow");
        expectNotification("recording-device-events");
        if (aAllowVideo)
          expected.push("Camera");
        if (aAllowAudio)
          expected.push("Microphone");
        expected = expected.join("And");
      }
      else {
        expectNotification("getUserMedia:response:deny");
        expectNotification("recording-window-ended");
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
        yield promiseNotification("getUserMedia:request", gum);
        yield promisePopupNotification("webRTC-shareDevices");

        // Deny the request to cleanup...
        yield promiseMessage("error: PERMISSION_DENIED", () => {
          activateSecondaryAction(kActionDeny);
        });
        expectNotification("getUserMedia:response:deny");
        expectNotification("recording-window-ended");
      }
      else {
        let allow = (aAllowVideo && aRequestVideo) || (aAllowAudio && aRequestAudio);
        let expectedMessage = allow ? "ok" : "error: PERMISSION_DENIED";
        yield promiseMessage(expectedMessage, gum);

        if (expectedMessage == "ok") {
          expectNotification("recording-device-events");

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
          expectNotification("recording-window-ended");
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
    yield usePerm(false, true, true, false, true);
    info("deny audio, allow video, request video, expect ok (video)");
    yield usePerm(false, true, false, true, false);

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
      expectNotification("recording-device-events");
      yield checkSharingUI();

      // Stop sharing.
      PopupNotifications.getNotification("webRTC-sharingDevices").reshow();
      activateSecondaryAction(kActionDeny);

      yield promiseNotification("recording-device-events");
      expectNotification("getUserMedia:revoke");

      yield promiseNoPopupNotification("webRTC-sharingDevices");

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

    Task.spawn(function () {
      for (let test of gTests) {
        info(test.desc);
        yield test.run();

        // Cleanup before the next test
        expectNoNotifications();
      }
    }).then(finish, ex => {
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
  }, true);
  let rootDir = getRootDirectory(gTestPath)
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "http://127.0.0.1:8888/");
  content.location = rootDir + "get_user_media.html";
}


function wait(time) {
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, time);
  return deferred.promise;
}
