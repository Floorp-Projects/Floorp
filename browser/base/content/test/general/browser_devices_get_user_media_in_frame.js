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

function* closeStream(aGlobal, aAlreadyClosed) {
  expectNoObserverCalled();

  info("closing the stream");
  aGlobal.closeStream();

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

function getFrameGlobal(aFrameId) {
  return content.wrappedJSObject.document.getElementById(aFrameId).contentWindow;
}

const permissionError = "error: PermissionDeniedError: The user did not grant permission for the operation.";

let gTests = [

{
  desc: "getUserMedia audio+video",
  run: function checkAudioVideo() {
    let global = getFrameGlobal("frame1");
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      global.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareDevices-notification-icon", "anchored to device icon");
    checkDeviceSelectors(true, true);
    is(PopupNotifications.panel.firstChild.getAttribute("popupid"),
       "webRTC-shareDevices", "panel using devices icon");

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({audio: true, video: true});
    yield closeStream(global);
  }
},

{
  desc: "getUserMedia audio+video: stop sharing",
  run: function checkStopSharing() {
    let global = getFrameGlobal("frame1");
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      global.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      activateSecondaryAction(kActionAlways);
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    let Perms = Services.perms;
    let uri = Services.io.newURI("https://example.com/", null, null);
    is(Perms.testExactPermission(uri, "microphone"), Perms.ALLOW_ACTION,
                                 "microphone persistently allowed");
    is(Perms.testExactPermission(uri, "camera"), Perms.ALLOW_ACTION,
                                 "camera persistently allowed");

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

    // The persistent permissions for the frame should have been removed.
    is(Perms.testExactPermission(uri, "microphone"), Perms.UNKNOWN_ACTION,
                                 "microphone not persistently allowed");
    is(Perms.testExactPermission(uri, "camera"), Perms.UNKNOWN_ACTION,
                                 "camera not persistently allowed");

    // the stream is already closed, but this will do some cleanup anyway
    yield closeStream(global, true);
  }
},

{
  desc: "getUserMedia audio+video: reloading the frame removes all sharing UI",
  run: function checkReloading() {
    let global = getFrameGlobal("frame1");
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      global.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: true, audio: true});

    info("reloading the frame");
    yield promiseObserverCalled("recording-device-events",
                                () => { global.location.reload(); });

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
  desc: "getUserMedia audio+video: reloading the frame removes prompts",
  run: function checkReloadingRemovesPrompts() {
    let global = getFrameGlobal("frame1");
    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting devices");
      global.requestDevice(true, true);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    info("reloading the frame");
    yield promiseObserverCalled("recording-window-ended",
                                () => { global.location.reload(); });

    yield promiseNoPopupNotification("webRTC-shareDevices");

    expectNoObserverCalled();
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video: reloading a frame updates the sharing UI",
  run: function checkUpdateWhenReloading() {
    // We'll share only the mic in the first frame, then share both in the
    // second frame, then reload the second frame. After each step, we'll check
    // the UI is in the correct state.
    let g1 = getFrameGlobal("frame1"), g2 = getFrameGlobal("frame2");

    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting microphone in the first frame");
      g1.requestDevice(true, false);
    });
    expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, false);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    expectObserverCalled("getUserMedia:response:allow");
    expectObserverCalled("recording-device-events");
    is(getMediaCaptureState(), "Microphone", "microphone to be shared");

    yield indicator;
    yield checkSharingUI({video: false, audio: true});
    expectNoObserverCalled();

    yield promisePopupNotificationShown("webRTC-shareDevices", () => {
      info("requesting both devices in the second frame");
      g2.requestDevice(true, true);
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
    expectNoObserverCalled();

    info("reloading the second frame");
    yield promiseObserverCalled("recording-device-events",
                                () => { g2.location.reload(); });

    yield checkSharingUI({video: false, audio: true});
    expectObserverCalled("recording-window-ended");
    if (gObservedTopics["recording-device-events"] == 1) {
      todo(false, "Got the 'recording-device-events' notification twice, likely because of bug 962719");
      gObservedTopics["recording-device-events"] = 0;
    }
    expectNoObserverCalled();

    yield closeStream(g1);
    yield promiseNoPopupNotification("webRTC-sharingDevices");
    expectNoObserverCalled();
    yield checkNotSharing();
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
  let url = rootDir + "get_user_media.html";
  content.location = 'data:text/html,<iframe id="frame1" src="' + url + '"></iframe><iframe id="frame2" src="' + url + '"></iframe>'
}
