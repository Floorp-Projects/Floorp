const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

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

const STATE_CAPTURE_ENABLED = Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED;
const STATE_CAPTURE_DISABLED = Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED;

let observerTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended",
];

function whenDelayedStartupFinished(aWindow) {
  return TestUtils.topicObserved(
    "browser-delayed-startup-finished",
    subject => subject == aWindow
  );
}

function promiseIndicatorWindow() {
  // We don't show the indicator window on Mac.
  if ("nsISystemStatusBar" in Ci) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    Services.obs.addObserver(function obs(win) {
      win.addEventListener(
        "load",
        function() {
          if (
            win.location.href !==
            "chrome://browser/content/webrtcIndicator.xhtml"
          ) {
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

function ignoreEvent(aSubject, aTopic, aData) {
  // With e10s disabled, our content script receives notifications for the
  // preview displayed in our screen sharing permission prompt; ignore them.
  const kBrowserURL = AppConstants.BROWSER_CHROME_URL;
  const nsIPropertyBag = Ci.nsIPropertyBag;
  if (
    aTopic == "recording-device-events" &&
    aSubject.QueryInterface(nsIPropertyBag).getProperty("requestURL") ==
      kBrowserURL
  ) {
    return true;
  }
  if (aTopic == "recording-window-ended") {
    let win = Services.wm.getOuterWindowWithId(aData).top;
    if (win.document.documentURI == kBrowserURL) {
      return true;
    }
  }
  return false;
}

function expectObserverCalledInProcess(aTopic, aCount = 1) {
  let promises = [];
  for (let count = aCount; count > 0; count--) {
    promises.push(TestUtils.topicObserved(aTopic, ignoreEvent));
  }
  return promises;
}

function expectObserverCalled(
  aTopic,
  aCount = 1,
  browser = gBrowser.selectedBrowser
) {
  if (!gMultiProcessBrowser) {
    return expectObserverCalledInProcess(aTopic, aCount);
  }

  return BrowserTestUtils.contentTopicObserved(
    browser.browsingContext,
    aTopic,
    aCount
  );
}

// This is a special version of expectObserverCalled that should only
// be used when expecting a notification upon closing a window. It uses
// the per-process message manager instead of actors to send the
// notifications.
function expectObserverCalledOnClose(
  aTopic,
  aCount = 1,
  browser = gBrowser.selectedBrowser
) {
  if (!gMultiProcessBrowser) {
    return expectObserverCalledInProcess(aTopic, aCount);
  }

  return new Promise(resolve => {
    BrowserTestUtils.sendAsyncMessage(
      browser.browsingContext,
      "BrowserTestUtils:ObserveTopic",
      {
        topic: aTopic,
        count: 1,
        filterFunctionSource: ((subject, topic, data) => {
          Services.cpmm.sendAsyncMessage("WebRTCTest:ObserverCalled", {
            topic,
          });
          return true;
        }).toSource(),
      }
    );

    function observerCalled(message) {
      if (message.data.topic == aTopic) {
        Services.ppmm.removeMessageListener(
          "WebRTCTest:ObserverCalled",
          observerCalled
        );
        resolve();
      }
    }
    Services.ppmm.addMessageListener(
      "WebRTCTest:ObserverCalled",
      observerCalled
    );
  });
}

function promiseMessage(aMessage, aAction, aCount = 1) {
  let promise = ContentTask.spawn(
    gBrowser.selectedBrowser,
    [aMessage, aCount],
    async function([expectedMessage, expectedCount]) {
      return new Promise(resolve => {
        function listenForMessage({ data }) {
          if (
            (!expectedMessage || data == expectedMessage) &&
            --expectedCount == 0
          ) {
            content.removeEventListener("message", listenForMessage);
            resolve(data);
          }
        }
        content.addEventListener("message", listenForMessage);
      });
    }
  );
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

async function promisePopupNotification(aName) {
  return TestUtils.waitForCondition(
    () => PopupNotifications.getNotification(aName),
    aName + " notification appeared"
  );
}

async function promiseNoPopupNotification(aName) {
  return TestUtils.waitForCondition(
    () => !PopupNotifications.getNotification(aName),
    aName + " notification removed"
  );
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

async function getMediaCaptureState() {
  function gatherBrowsingContexts(aBrowsingContext) {
    let list = [aBrowsingContext];

    let children = aBrowsingContext.getChildren();
    for (let child of children) {
      list.push(...gatherBrowsingContexts(child));
    }

    return list;
  }

  function combine(x, y) {
    if (
      x == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED ||
      y == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED
    ) {
      return Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED;
    }
    if (
      x == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED ||
      y == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED
    ) {
      return Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED;
    }
    return Ci.nsIMediaManagerService.STATE_NOCAPTURE;
  }

  let video = Ci.nsIMediaManagerService.STATE_NOCAPTURE;
  let audio = Ci.nsIMediaManagerService.STATE_NOCAPTURE;
  let screen = Ci.nsIMediaManagerService.STATE_NOCAPTURE;
  let window = Ci.nsIMediaManagerService.STATE_NOCAPTURE;
  let browser = Ci.nsIMediaManagerService.STATE_NOCAPTURE;

  for (let bc of gatherBrowsingContexts(
    gBrowser.selectedBrowser.browsingContext
  )) {
    let state = await SpecialPowers.spawn(bc, [], async function() {
      let mediaManagerService = Cc[
        "@mozilla.org/mediaManagerService;1"
      ].getService(Ci.nsIMediaManagerService);

      let hasCamera = {};
      let hasMicrophone = {};
      let hasScreenShare = {};
      let hasWindowShare = {};
      let hasBrowserShare = {};
      mediaManagerService.mediaCaptureWindowState(
        content,
        hasCamera,
        hasMicrophone,
        hasScreenShare,
        hasWindowShare,
        hasBrowserShare,
        false
      );

      return {
        video: hasCamera.value,
        audio: hasMicrophone.value,
        screen: hasScreenShare.value,
        window: hasWindowShare.value,
        browser: hasBrowserShare.value,
      };
    });

    video = combine(state.video, video);
    audio = combine(state.audio, audio);
    screen = combine(state.screen, screen);
    window = combine(state.window, window);
    browser = combine(state.browser, browser);
  }

  let result = {};

  if (video != Ci.nsIMediaManagerService.STATE_NOCAPTURE) {
    result.video = true;
  }
  if (audio != Ci.nsIMediaManagerService.STATE_NOCAPTURE) {
    result.audio = true;
  }

  if (screen != Ci.nsIMediaManagerService.STATE_NOCAPTURE) {
    result.screen = "Screen";
  } else if (window != Ci.nsIMediaManagerService.STATE_NOCAPTURE) {
    result.screen = "Window";
  } else if (browser != Ci.nsIMediaManagerService.STATE_NOCAPTURE) {
    result.screen = "Browser";
  }

  return result;
}

async function stopSharing(aType = "camera", aShouldKeepSharing = false) {
  let promiseRecordingEvent = expectObserverCalled("recording-device-events");
  gIdentityHandler._identityBox.click();
  let permissions = document.getElementById("identity-popup-permission-list");
  let cancelButton = permissions.querySelector(
    ".identity-popup-permission-icon." +
      aType +
      "-icon ~ " +
      ".identity-popup-permission-remove-button"
  );
  let observerPromise1 = expectObserverCalled("getUserMedia:revoke");

  // If we are stopping screen sharing and expect to still have another stream,
  // "recording-window-ended" won't be fired.
  let observerPromise2 = null;
  if (!aShouldKeepSharing) {
    observerPromise2 = expectObserverCalled("recording-window-ended");
  }

  cancelButton.click();
  gIdentityHandler._identityPopup.hidden = true;
  await promiseRecordingEvent;
  await observerPromise1;
  await observerPromise2;

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
  return SpecialPowers.spawn(
    aBrowser,
    [{ aRequestAudio, aRequestVideo, aFrameId, aType, aBadDevice }],
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

async function closeStream(
  aAlreadyClosed,
  aFrameId,
  aDontFlushObserverVerification
) {
  // Check that spurious notifications that occur while closing the
  // stream are handled separately. Tests that use skipObserverVerification
  // should pass true for aDontFlushObserverVerification.
  if (!aDontFlushObserverVerification) {
    await disableObserverVerification();
    await enableObserverVerification();
  }

  let observerPromises = [];
  if (!aAlreadyClosed) {
    observerPromises.push(expectObserverCalled("recording-device-events"));
    observerPromises.push(expectObserverCalled("recording-window-ended"));
  }

  info("closing the stream");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [aFrameId],
    async function(contentFrameId) {
      let global = content.wrappedJSObject;
      if (contentFrameId) {
        global = global.document.getElementById(contentFrameId).contentWindow;
      }
      global.closeStream();
    }
  );

  await Promise.all(observerPromises);

  await assertWebRTCIndicatorStatus(null);
}

async function reloadAndAssertClosedStreams() {
  info("reloading the web page");

  // Disable observers as the page is being reloaded which can destroy
  // the actors listening to the notifications.
  await disableObserverVerification();

  let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () =>
    content.location.reload()
  );

  await loadedPromise;

  await enableObserverVerification();

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
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser.browsingContext,
    [aFrameId],
    async function(contentFrameId) {
      let frame = content.wrappedJSObject.document.getElementById(
        contentFrameId
      );
      return new Promise(resolve => {
        function listener() {
          frame.removeEventListener("load", listener, true);
          resolve();
        }
        frame.addEventListener("load", listener, true);

        content.wrappedJSObject.document
          .getElementById(contentFrameId)
          .contentWindow.location.reload();
      });
    }
  );
}

function promiseChangeLocationFrame(aFrameId, aNewLocation) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser.browsingContext,
    [{ aFrameId, aNewLocation }],
    async function(args) {
      let frame = content.wrappedJSObject.document.getElementById(
        args.aFrameId
      );
      return new Promise(resolve => {
        function listener() {
          frame.removeEventListener("load", listener, true);
          resolve();
        }
        frame.addEventListener("load", listener, true);

        content.wrappedJSObject.document.getElementById(
          args.aFrameId
        ).contentWindow.location = args.aNewLocation;
      });
    }
  );
}

async function openNewTestTab(leaf = "get_user_media.html") {
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  );
  let absoluteURI = rootDir + leaf;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, absoluteURI);
  return tab.linkedBrowser;
}

// Enabling observer verification adds listeners for all of the webrtc
// observer topics. If any notifications occur for those topics that
// were not explicitly requested, a failure will occur.
function enableObserverVerification(browser = gBrowser.selectedBrowser) {
  // Skip these checks in single process mode as it isn't worth implementing it.
  if (!gMultiProcessBrowser) {
    return Promise.resolve();
  }

  return BrowserTestUtils.startObservingTopics(
    browser.browsingContext,
    observerTopics
  );
}

function disableObserverVerification(browser = gBrowser.selectedBrowser) {
  if (!gMultiProcessBrowser) {
    return Promise.resolve();
  }

  return BrowserTestUtils.stopObservingTopics(
    browser.browsingContext,
    observerTopics
  ).catch(reason => {
    ok(false, "Failed " + reason);
  });
}

async function runTests(tests, options = {}) {
  let browser = await openNewTestTab(options.relativeURI);

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
    if (
      !testCase.skipObserverVerification &&
      !options.skipObserverVerification
    ) {
      await enableObserverVerification();
    }
    await testCase.run(browser);
    if (
      !testCase.skipObserverVerification &&
      !options.skipObserverVerification
    ) {
      await disableObserverVerification();
    }
    if (options.cleanup) {
      await options.cleanup();
    }
  }

  // Some tests destroy the original tab and leave a new one in its place.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}
