Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

const PREF_PERMISSION_FAKE = "media.navigator.permission.fake";
const CONTENT_SCRIPT_HELPER = getRootDirectory(gTestPath) + "get_user_media_content_script.js";

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== 'undefined' ?  retryTimes : 30;
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= retryTimes) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function promiseWaitForCondition(aConditionFn) {
  let deferred = Promise.defer();
  waitForCondition(aConditionFn, deferred.resolve, "Condition didn't pass.");
  return deferred.promise;
}

/**
 * Waits for a window with the given URL to exist.
 *
 * @param url
 *        The url of the window.
 * @return {Promise} resolved when the window exists.
 * @resolves to the window
 */
function promiseWindow(url) {
  info("expecting a " + url + " window");
  return new Promise(resolve => {
    Services.obs.addObserver(function obs(win) {
      win.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function loadHandler() {
        win.removeEventListener("load", loadHandler);

        if (win.location.href !== url) {
          info("ignoring a window with this url: " + win.location.href);
          return;
        }

        Services.obs.removeObserver(obs, "domwindowopened");
        resolve(win);
      });
    }, "domwindowopened", false);
  });
}

function whenDelayedStartupFinished(aWindow) {
  return new Promise(resolve => {
    info("Waiting for delayed startup to finish");
    Services.obs.addObserver(function observer(aSubject, aTopic) {
      if (aWindow == aSubject) {
        Services.obs.removeObserver(observer, aTopic);
        resolve();
      }
    }, "browser-delayed-startup-finished", false);
  });
}

function promiseIndicatorWindow() {
  // We don't show the indicator window on Mac.
  if ("nsISystemStatusBar" in Ci)
    return Promise.resolve();

  return promiseWindow("chrome://browser/content/webrtcIndicator.xul");
}

function* assertWebRTCIndicatorStatus(expected) {
  let ui = Cu.import("resource:///modules/webrtcUI.jsm", {}).webrtcUI;
  let expectedState = expected ? "visible" : "hidden";
  let msg = "WebRTC indicator " + expectedState;
  if (!expected && ui.showGlobalIndicator) {
    // It seems the global indicator is not always removed synchronously
    // in some cases.
    info("waiting for the global indicator to be hidden");
    yield promiseWaitForCondition(() => !ui.showGlobalIndicator);
  }
  is(ui.showGlobalIndicator, !!expected, msg);

  let expectVideo = false, expectAudio = false, expectScreen = false;
  if (expected) {
    if (expected.video)
      expectVideo = true;
    if (expected.audio)
      expectAudio = true;
    if (expected.screen)
      expectScreen = true;
  }
  is(ui.showCameraIndicator, expectVideo, "camera global indicator as expected");
  is(ui.showMicrophoneIndicator, expectAudio, "microphone global indicator as expected");
  is(ui.showScreenSharingIndicator, expectScreen, "screen global indicator as expected");

  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    let menu = win.document.getElementById("tabSharingMenu");
    is(menu && !menu.hidden, !!expected, "WebRTC menu should be " + expectedState);
  }

  if (!("nsISystemStatusBar" in Ci)) {
    if (!expected) {
      let win = Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
      if (win) {
        yield new Promise((resolve, reject) => {
          win.addEventListener("unload", function listener(e) {
            if (e.target == win.document) {
              win.removeEventListener("unload", listener);
              resolve();
            }
          }, false);
        });
      }
    }
    let indicator = Services.wm.getEnumerator("Browser:WebRTCGlobalIndicator");
    let hasWindow = indicator.hasMoreElements();
    is(hasWindow, !!expected, "popup " + msg);
    if (hasWindow) {
      let document = indicator.getNext().document;
      let docElt = document.documentElement;

      if (document.readyState != "complete") {
        info("Waiting for the sharing indicator's document to load");
        let deferred = Promise.defer();
        document.addEventListener("readystatechange",
                                  function onReadyStateChange() {
          if (document.readyState != "complete")
            return;
          document.removeEventListener("readystatechange", onReadyStateChange);
          deferred.resolve();
        });
        yield deferred.promise;
      }

      for (let item of ["video", "audio", "screen"]) {
        let expectedValue = (expected && expected[item]) ? "true" : "";
        is(docElt.getAttribute("sharing" + item), expectedValue,
           item + " global indicator attribute as expected");
      }

      ok(!indicator.hasMoreElements(), "only one global indicator window");
    }
  }
}

function promisePopupEvent(popup, eventSuffix) {
  let endState = {shown: "open", hidden: "closed"}[eventSuffix];

  if (popup.state == endState)
    return Promise.resolve();

  let eventType = "popup" + eventSuffix;
  let deferred = Promise.defer();
  popup.addEventListener(eventType, function onPopupShown(event) {
    popup.removeEventListener(eventType, onPopupShown);
    deferred.resolve();
  });

  return deferred.promise;
}

function promiseNotificationShown(notification) {
  let win = notification.browser.ownerGlobal;
  if (win.PopupNotifications.panel.state == "open") {
    return Promise.resolve();
  }
  let panelPromise = promisePopupEvent(win.PopupNotifications.panel, "shown");
  notification.reshow();
  return panelPromise;
}

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

function promiseMessage(aMessage, aAction) {
  let promise = new Promise((resolve, reject) => {
    let mm = _mm();
    mm.addMessageListener("Test:MessageReceived", function listener({data}) {
      is(data, aMessage, "received " + aMessage);
      if (data == aMessage)
        resolve();
      else
        reject();
      mm.removeMessageListener("Test:MessageReceived", listener);
    });
    mm.sendAsyncMessage("Test:WaitForMessage");
  });

  if (aAction)
    aAction();

  return promise;
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

function getMediaCaptureState() {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:MediaCaptureState", ({data}) => {
      resolve(data);
    });
    mm.sendAsyncMessage("Test:GetMediaCaptureState");
  });
}

function* stopSharing(aType = "camera") {
  let promiseRecordingEvent = promiseObserverCalled("recording-device-events");
  gIdentityHandler._identityBox.click();
  let permissions = document.getElementById("identity-popup-permission-list");
  let cancelButton =
    permissions.querySelector(".identity-popup-permission-icon." + aType + "-icon ~ " +
                              ".identity-popup-permission-remove-button");
  cancelButton.click();
  gIdentityHandler._identityPopup.hidden = true;
  yield promiseRecordingEvent;
  yield expectObserverCalled("getUserMedia:revoke");
  yield expectObserverCalled("recording-window-ended");
  yield expectNoObserverCalled();
  yield* checkNotSharing();
}

function promiseRequestDevice(aRequestAudio, aRequestVideo, aFrameId, aType) {
  info("requesting devices");
  return ContentTask.spawn(gBrowser.selectedBrowser,
                           {aRequestAudio, aRequestVideo, aFrameId, aType},
                           function*(args) {
    let global = content.wrappedJSObject;
    if (args.aFrameId)
      global = global.document.getElementById(args.aFrameId).contentWindow;
    global.requestDevice(args.aRequestAudio, args.aRequestVideo, args.aType);
  });
}

function* closeStream(aAlreadyClosed, aFrameId) {
  yield expectNoObserverCalled();

  let promises;
  if (!aAlreadyClosed) {
    promises = [promiseObserverCalled("recording-device-events"),
                promiseObserverCalled("recording-window-ended")];
  }

  info("closing the stream");
  yield ContentTask.spawn(gBrowser.selectedBrowser, aFrameId, function*(aFrameId) {
    let global = content.wrappedJSObject;
    if (aFrameId)
      global = global.document.getElementById(aFrameId).contentWindow;
    global.closeStream();
  });

  if (promises)
    yield Promise.all(promises);

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

function* checkSharingUI(aExpected, aWin = window) {
  let doc = aWin.document;
  // First check the icon above the control center (i) icon.
  let identityBox = doc.getElementById("identity-box");
  ok(identityBox.hasAttribute("sharing"), "sharing attribute is set");
  let sharing = identityBox.getAttribute("sharing");
  if (aExpected.video)
    is(sharing, "camera", "showing camera icon on the control center icon");
  else if (aExpected.audio)
    is(sharing, "microphone", "showing mic icon on the control center icon");

  // Then check the sharing indicators inside the control center panel.
  identityBox.click();
  let permissions = doc.getElementById("identity-popup-permission-list");
  for (let id of ["microphone", "camera", "screen"]) {
    let convertId = id => {
      if (id == "camera")
        return "video";
      if (id == "microphone")
        return "audio";
      return id;
    };
    let expected = aExpected[convertId(id)];
    is(!!aWin.gIdentityHandler._sharingState[id], !!expected,
       "sharing state for " + id + " as expected");
    let icon = permissions.querySelectorAll(
      ".identity-popup-permission-icon." + id + "-icon");
    if (expected) {
      is(icon.length, 1, "should show " + id + " icon in control center panel");
      ok(icon[0].classList.contains("in-use"), "icon should have the in-use class");
    } else if (!icon.length) {
      ok(true, "should not show " + id + " icon in the control center panel");
    } else {
      // This will happen if there are persistent permissions set.
      ok(!icon[0].classList.contains("in-use"),
         "if shown, the " + id + " icon should not have the in-use class");
      is(icon.length, 1, "should not show more than 1 " + id + " icon");
    }
  }
  aWin.gIdentityHandler._identityPopup.hidden = true;

  // Check the global indicators.
  yield* assertWebRTCIndicatorStatus(aExpected);
}

function* checkNotSharing() {
  is((yield getMediaCaptureState()), "none", "expected nothing to be shared");

  ok(!document.getElementById("identity-box").hasAttribute("sharing"),
     "no sharing indicator on the control center icon");

  yield* assertWebRTCIndicatorStatus(null);
}
