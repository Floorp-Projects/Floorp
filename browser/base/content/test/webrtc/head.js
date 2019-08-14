var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
var { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const PREF_PERMISSION_FAKE = "media.navigator.permission.fake";
const PREF_AUDIO_LOOPBACK = "media.audio_loopback_dev";
const PREF_VIDEO_LOOPBACK = "media.video_loopback_dev";
const PREF_FAKE_STREAMS = "media.navigator.streams.fake";
const PREF_FOCUS_SOURCE = "media.getusermedia.window.focus_source.enabled";
const CONTENT_SCRIPT_HELPER =
  getRootDirectory(gTestPath) + "get_user_media_content_script.js";

const STATE_CAPTURE_ENABLED = Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED;
const STATE_CAPTURE_DISABLED = Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED;

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== "undefined" ? retryTimes : 30;
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
  var moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
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
      win.addEventListener(
        "load",
        function() {
          if (win.location.href !== url) {
            info("ignoring a window with this url: " + win.location.href);
            return;
          }

          Services.obs.removeObserver(obs, "domwindowopened");
          executeSoon(() => resolve(win));
        },
        { once: true }
      );
    }, "domwindowopened");
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
    }, "browser-delayed-startup-finished");
  });
}

function promiseIndicatorWindow() {
  // We don't show the indicator window on Mac.
  if ("nsISystemStatusBar" in Ci) {
    return Promise.resolve();
  }

  return promiseWindow("chrome://browser/content/webrtcIndicator.xul");
}

async function assertWebRTCIndicatorStatus(expected) {
  let ui = ChromeUtils.import("resource:///modules/webrtcUI.jsm", {}).webrtcUI;
  let expectedState = expected ? "visible" : "hidden";
  let msg = "WebRTC indicator " + expectedState;
  if (!expected && ui.showGlobalIndicator) {
    // It seems the global indicator is not always removed synchronously
    // in some cases.
    info("waiting for the global indicator to be hidden");
    await TestUtils.waitForCondition(() => !ui.showGlobalIndicator);
  }
  is(ui.showGlobalIndicator, !!expected, msg);

  let expectVideo = false,
    expectAudio = false,
    expectScreen = false;
  if (expected) {
    if (expected.video) {
      expectVideo = true;
    }
    if (expected.audio) {
      expectAudio = true;
    }
    if (expected.screen) {
      expectScreen = expected.screen;
    }
  }
  is(
    ui.showCameraIndicator,
    expectVideo,
    "camera global indicator as expected"
  );
  is(
    ui.showMicrophoneIndicator,
    expectAudio,
    "microphone global indicator as expected"
  );
  is(
    ui.showScreenSharingIndicator,
    expectScreen,
    "screen global indicator as expected"
  );

  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    let menu = win.document.getElementById("tabSharingMenu");
    is(
      !!menu && !menu.hidden,
      !!expected,
      "WebRTC menu should be " + expectedState
    );
  }

  if (!("nsISystemStatusBar" in Ci)) {
    if (!expected) {
      let win = Services.wm.getMostRecentWindow(
        "Browser:WebRTCGlobalIndicator"
      );
      if (win) {
        await new Promise((resolve, reject) => {
          win.addEventListener("unload", function listener(e) {
            if (e.target == win.document) {
              win.removeEventListener("unload", listener);
              executeSoon(resolve);
            }
          });
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
        await new Promise(resolve => {
          document.addEventListener(
            "readystatechange",
            function onReadyStateChange() {
              if (document.readyState != "complete") {
                return;
              }
              document.removeEventListener(
                "readystatechange",
                onReadyStateChange
              );
              executeSoon(resolve);
            }
          );
        });
      }

      for (let item of ["video", "audio", "screen"]) {
        let expectedValue = expected && expected[item] ? "true" : "";
        is(
          docElt.getAttribute("sharing" + item),
          expectedValue,
          item + " global indicator attribute as expected"
        );
      }

      ok(!indicator.hasMoreElements(), "only one global indicator window");
    }
  }
}

function promiseNotificationShown(notification) {
  let win = notification.browser.ownerGlobal;
  if (win.PopupNotifications.panel.state == "open") {
    return Promise.resolve();
  }
  let panelPromise = BrowserTestUtils.waitForPopupEvent(
    win.PopupNotifications.panel,
    "shown"
  );
  notification.reshow();
  return panelPromise;
}

function _mm() {
  return gBrowser.selectedBrowser.messageManager;
}

function promiseObserverCalled(aTopic) {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:ObserverCalled", function listener({ data }) {
      if (data == aTopic) {
        ok(true, "got " + aTopic + " notification");
        mm.removeMessageListener("Test:ObserverCalled", listener);
        resolve();
      }
    });
    mm.sendAsyncMessage("Test:WaitForObserverCall", aTopic);
  });
}

function expectObserverCalled(aTopic, aCount = 1) {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:ExpectObserverCalled:Reply", function listener({
      data,
    }) {
      is(data.count, aCount, "expected notification " + aTopic);
      mm.removeMessageListener("Test:ExpectObserverCalled:Reply", listener);
      resolve();
    });
    mm.sendAsyncMessage("Test:ExpectObserverCalled", {
      topic: aTopic,
      count: aCount,
    });
  });
}

function expectNoObserverCalled() {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener(
      "Test:ExpectNoObserverCalled:Reply",
      function listener({ data }) {
        mm.removeMessageListener("Test:ExpectNoObserverCalled:Reply", listener);
        for (let topic in data) {
          if (!data[topic]) {
            continue;
          }

          is(data[topic], 0, topic + " notification unexpected");
        }
        resolve();
      }
    );
    mm.sendAsyncMessage("Test:ExpectNoObserverCalled");
  });
}

function ignoreObserversCalled() {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener(
      "Test:ExpectNoObserverCalled:Reply",
      function listener() {
        mm.removeMessageListener("Test:ExpectNoObserverCalled:Reply", listener);
        resolve();
      }
    );
    mm.sendAsyncMessage("Test:ExpectNoObserverCalled");
  });
}

function promiseMessageReceived() {
  return new Promise((resolve, reject) => {
    let mm = _mm();
    mm.addMessageListener("Test:MessageReceived", function listener({ data }) {
      mm.removeMessageListener("Test:MessageReceived", listener);
      resolve(data);
    });
    mm.sendAsyncMessage("Test:WaitForMessage");
  });
}

function promiseSpecificMessageReceived(aMessage, aCount = 1) {
  return new Promise(resolve => {
    let mm = _mm();
    let counter = 0;
    mm.addMessageListener("Test:MessageReceived", function listener({ data }) {
      if (data == aMessage) {
        counter++;
        if (counter == aCount) {
          mm.sendAsyncMessage("Test:StopWaitForMultipleMessages");
          mm.removeMessageListener("Test:MessageReceived", listener);
          resolve(data);
        }
      }
    });
    mm.sendAsyncMessage("Test:WaitForMultipleMessages");
  });
}

function promiseMessage(aMessage, aAction) {
  let promise = new Promise((resolve, reject) => {
    promiseMessageReceived(aAction).then(data => {
      is(data, aMessage, "received " + aMessage);
      if (data == aMessage) {
        resolve();
      } else {
        reject();
      }
    });
  });

  if (aAction) {
    aAction();
  }

  return promise;
}

function promisePopupNotificationShown(aName, aAction) {
  return new Promise(resolve => {
    // In case the global webrtc indicator has stolen focus (bug 1421724)
    window.focus();

    PopupNotifications.panel.addEventListener(
      "popupshown",
      function() {
        ok(
          !!PopupNotifications.getNotification(aName),
          aName + " notification shown"
        );
        ok(PopupNotifications.isPanelOpen, "notification panel open");
        ok(
          !!PopupNotifications.panel.firstElementChild,
          "notification panel populated"
        );

        executeSoon(resolve);
      },
      { once: true }
    );

    if (aAction) {
      aAction();
    }
  });
}

function promisePopupNotification(aName) {
  return new Promise(resolve => {
    waitForCondition(
      () => PopupNotifications.getNotification(aName),
      () => {
        ok(
          !!PopupNotifications.getNotification(aName),
          aName + " notification appeared"
        );

        resolve();
      },
      "timeout waiting for popup notification " + aName
    );
  });
}

function promiseNoPopupNotification(aName) {
  return new Promise(resolve => {
    waitForCondition(
      () => !PopupNotifications.getNotification(aName),
      () => {
        ok(
          !PopupNotifications.getNotification(aName),
          aName + " notification removed"
        );
        resolve();
      },
      "timeout waiting for popup notification " + aName + " to disappear"
    );
  });
}

const kActionAlways = 1;
const kActionDeny = 2;
const kActionNever = 3;

function activateSecondaryAction(aAction) {
  let notification = PopupNotifications.panel.firstElementChild;
  switch (aAction) {
    case kActionNever:
      if (!notification.checkbox.checked) {
        notification.checkbox.click();
      }
    // fallthrough
    case kActionDeny:
      notification.secondaryButton.click();
      break;
    case kActionAlways:
      if (!notification.checkbox.checked) {
        notification.checkbox.click();
      }
      notification.button.click();
      break;
  }
}

function getMediaCaptureState() {
  return new Promise(resolve => {
    let mm = _mm();
    mm.addMessageListener("Test:MediaCaptureState", ({ data }) => {
      resolve(data);
    });
    mm.sendAsyncMessage("Test:GetMediaCaptureState");
  });
}

async function stopSharing(aType = "camera", aShouldKeepSharing = false) {
  let promiseRecordingEvent = promiseObserverCalled("recording-device-events");
  gIdentityHandler._identityBox.click();
  let permissions = document.getElementById("identity-popup-permission-list");
  let cancelButton = permissions.querySelector(
    ".identity-popup-permission-icon." +
      aType +
      "-icon ~ " +
      ".identity-popup-permission-remove-button"
  );
  cancelButton.click();
  gIdentityHandler._identityPopup.hidden = true;
  await promiseRecordingEvent;
  await expectObserverCalled("getUserMedia:revoke");

  // If we are stopping screen sharing and expect to still have another stream,
  // "recording-window-ended" won't be fired.
  if (!aShouldKeepSharing) {
    await expectObserverCalled("recording-window-ended");
  }

  await expectNoObserverCalled();

  if (!aShouldKeepSharing) {
    await checkNotSharing();
  }
}

function promiseRequestDevice(
  aRequestAudio,
  aRequestVideo,
  aFrameId,
  aType,
  aBrowser = gBrowser.selectedBrowser,
  aBadDevice = false
) {
  info("requesting devices");
  return ContentTask.spawn(
    aBrowser,
    { aRequestAudio, aRequestVideo, aFrameId, aType, aBadDevice },
    async function(args) {
      let global = content.wrappedJSObject;
      if (args.aFrameId) {
        global = global.document.getElementById(args.aFrameId).contentWindow;
      }
      global.requestDevice(
        args.aRequestAudio,
        args.aRequestVideo,
        args.aType,
        args.aBadDevice
      );
    }
  );
}

async function closeStream(aAlreadyClosed, aFrameId) {
  await expectNoObserverCalled();

  let promises;
  if (!aAlreadyClosed) {
    promises = [
      promiseObserverCalled("recording-device-events"),
      promiseObserverCalled("recording-window-ended"),
    ];
  }

  info("closing the stream");
  await ContentTask.spawn(gBrowser.selectedBrowser, aFrameId, async function(
    contentFrameId
  ) {
    let global = content.wrappedJSObject;
    if (contentFrameId) {
      global = global.document.getElementById(contentFrameId).contentWindow;
    }
    global.closeStream();
  });

  if (promises) {
    await Promise.all(promises);
  }

  await assertWebRTCIndicatorStatus(null);
}

async function reloadAndAssertClosedStreams() {
  info("reloading the web page");
  let promises = [
    promiseObserverCalled("recording-device-events"),
    promiseObserverCalled("recording-window-ended"),
  ];
  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    "() => content.location.reload()"
  );
  await Promise.all(promises);

  await expectNoObserverCalled();
  await checkNotSharing();
}

function checkDeviceSelectors(aAudio, aVideo, aScreen) {
  let micSelector = document.getElementById("webRTC-selectMicrophone");
  if (aAudio) {
    ok(!micSelector.hidden, "microphone selector visible");
  } else {
    ok(micSelector.hidden, "microphone selector hidden");
  }

  let cameraSelector = document.getElementById("webRTC-selectCamera");
  if (aVideo) {
    ok(!cameraSelector.hidden, "camera selector visible");
  } else {
    ok(cameraSelector.hidden, "camera selector hidden");
  }

  let screenSelector = document.getElementById("webRTC-selectWindowOrScreen");
  if (aScreen) {
    ok(!screenSelector.hidden, "screen selector visible");
  } else {
    ok(screenSelector.hidden, "screen selector hidden");
  }
}

// aExpected is for the current tab,
// aExpectedGlobal is for all tabs.
async function checkSharingUI(
  aExpected,
  aWin = window,
  aExpectedGlobal = null
) {
  function isPaused(streamState) {
    if (typeof streamState == "string") {
      return streamState.includes("Paused");
    }
    return streamState == STATE_CAPTURE_DISABLED;
  }

  let doc = aWin.document;
  // First check the icon above the control center (i) icon.
  let identityBox = doc.getElementById("identity-box");
  let webrtcSharingIcon = doc.getElementById("webrtc-sharing-icon");
  ok(webrtcSharingIcon.hasAttribute("sharing"), "sharing attribute is set");
  let sharing = webrtcSharingIcon.getAttribute("sharing");
  if (aExpected.screen) {
    is(sharing, "screen", "showing screen icon in the identity block");
  } else if (aExpected.video == STATE_CAPTURE_ENABLED) {
    is(sharing, "camera", "showing camera icon in the identity block");
  } else if (aExpected.audio == STATE_CAPTURE_ENABLED) {
    is(sharing, "microphone", "showing mic icon in the identity block");
  } else if (aExpected.video) {
    is(sharing, "camera", "showing camera icon in the identity block");
  } else if (aExpected.audio) {
    is(sharing, "microphone", "showing mic icon in the identity block");
  }

  let allStreamsPaused = Object.values(aExpected).every(isPaused);
  is(
    webrtcSharingIcon.hasAttribute("paused"),
    allStreamsPaused,
    "sharing icon(s) should be in paused state when paused"
  );

  // Then check the sharing indicators inside the control center panel.
  identityBox.click();
  let permissions = doc.getElementById("identity-popup-permission-list");
  for (let id of ["microphone", "camera", "screen"]) {
    let convertId = idToConvert => {
      if (idToConvert == "camera") {
        return "video";
      }
      if (idToConvert == "microphone") {
        return "audio";
      }
      return idToConvert;
    };
    let expected = aExpected[convertId(id)];
    is(
      !!aWin.gIdentityHandler._sharingState.webRTC[id],
      !!expected,
      "sharing state for " + id + " as expected"
    );
    let icon = permissions.querySelectorAll(
      ".identity-popup-permission-icon." + id + "-icon"
    );
    if (expected) {
      is(icon.length, 1, "should show " + id + " icon in control center panel");
      is(
        icon[0].classList.contains("in-use"),
        expected && !isPaused(expected),
        "icon should have the in-use class, unless paused"
      );
    } else if (!icon.length) {
      ok(true, "should not show " + id + " icon in the control center panel");
    } else {
      // This will happen if there are persistent permissions set.
      ok(
        !icon[0].classList.contains("in-use"),
        "if shown, the " + id + " icon should not have the in-use class"
      );
      is(icon.length, 1, "should not show more than 1 " + id + " icon");
    }
  }
  aWin.gIdentityHandler._identityPopup.hidden = true;

  // Check the global indicators.
  await assertWebRTCIndicatorStatus(aExpectedGlobal || aExpected);
}

async function checkNotSharing() {
  Assert.deepEqual(
    await getMediaCaptureState(),
    {},
    "expected nothing to be shared"
  );

  ok(
    !document.getElementById("webrtc-sharing-icon").hasAttribute("sharing"),
    "no sharing indicator on the control center icon"
  );

  await assertWebRTCIndicatorStatus(null);
}

function promiseReloadFrame(aFrameId) {
  return ContentTask.spawn(gBrowser.selectedBrowser, aFrameId, async function(
    contentFrameId
  ) {
    content.wrappedJSObject.document
      .getElementById(contentFrameId)
      .contentWindow.location.reload();
  });
}

async function runTests(tests, options = {}) {
  let leaf = options.relativeURI || "get_user_media.html";

  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  );
  let absoluteURI = rootDir + leaf;
  let cleanup = options.cleanup || (() => expectNoObserverCalled());

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, absoluteURI);
  let browser = tab.linkedBrowser;

  browser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  is(
    PopupNotifications._currentNotifications.length,
    0,
    "should start the test without any prior popup notification"
  );
  ok(
    gIdentityHandler._identityPopup.hidden,
    "should start the test with the control center hidden"
  );

  // Set prefs so that permissions prompts are shown and loopback devices
  // are not used. To test the chrome we want prompts to be shown, and
  // these tests are flakey when using loopback devices (though it would
  // be desirable to make them work with loopback in future).
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  for (let testCase of tests) {
    info(testCase.desc);
    await testCase.run(browser);
    await cleanup();
  }

  // Some tests destroy the original tab and leave a new one in its place.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}
