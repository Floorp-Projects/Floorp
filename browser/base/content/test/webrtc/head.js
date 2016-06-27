Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

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

function promiseIndicatorWindow() {
  // We don't show the indicator window on Mac.
  if ("nsISystemStatusBar" in Ci)
    return Promise.resolve();

  return promiseWindow("chrome://browser/content/webrtcIndicator.xul");
}

function assertWebRTCIndicatorStatus(expected) {
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
  let win = notification.browser.ownerDocument.defaultView;
  if (win.PopupNotifications.panel.state == "open") {
    return Promise.resolve();
  }
  let panelPromise = promisePopupEvent(win.PopupNotifications.panel, "shown");
  notification.reshow();
  return panelPromise;
}
