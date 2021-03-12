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

const USING_LEGACY_INDICATOR = Services.prefs.getBoolPref(
  "privacy.webrtc.legacyGlobalIndicator",
  false
);

const ALLOW_SILENCING_NOTIFICATIONS = Services.prefs.getBoolPref(
  "privacy.webrtc.allowSilencingNotifications",
  false
);

const SHOW_GLOBAL_MUTE_TOGGLES = Services.prefs.getBoolPref(
  "privacy.webrtc.globalMuteToggles",
  false
);

const INDICATOR_PATH = USING_LEGACY_INDICATOR
  ? "chrome://browser/content/webrtcLegacyIndicator.xhtml"
  : "chrome://browser/content/webrtcIndicator.xhtml";

const IS_MAC = AppConstants.platform == "macosx";

const SHARE_SCREEN = 1;
const SHARE_WINDOW = 2;

let observerTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended",
];

// Structured hierarchy of subframes. Keys are frame id:s, The children member
// contains nested sub frames if any. The noTest member make a frame be ignored
// for testing if true.
let gObserveSubFrames = {};
// Object of subframes to test. Each element contains the members bc and id, for
// the frames BrowsingContext and id, respectively.
let gSubFramesToTest = [];
let gBrowserContextsToObserve = [];

function whenDelayedStartupFinished(aWindow) {
  return TestUtils.topicObserved(
    "browser-delayed-startup-finished",
    subject => subject == aWindow
  );
}

function promiseIndicatorWindow() {
  let startTime = performance.now();

  // We don't show the legacy indicator window on Mac.
  if (USING_LEGACY_INDICATOR && IS_MAC) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    Services.obs.addObserver(function obs(win) {
      win.addEventListener(
        "load",
        function() {
          if (win.location.href !== INDICATOR_PATH) {
            info("ignoring a window with this url: " + win.location.href);
            return;
          }

          Services.obs.removeObserver(obs, "domwindowopened");
          executeSoon(() => {
            ChromeUtils.addProfilerMarker("promiseIndicatorWindow", {
              startTime,
              category: "Test",
            });
            resolve(win);
          });
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
    await TestUtils.waitForCondition(
      () => !ui.showGlobalIndicator,
      "waiting for the global indicator to be hidden"
    );
  }
  is(ui.showGlobalIndicator, !!expected, msg);

  let expectVideo = false,
    expectAudio = false,
    expectScreen = "";
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
    Boolean(ui.showCameraIndicator),
    expectVideo,
    "camera global indicator as expected"
  );
  is(
    Boolean(ui.showMicrophoneIndicator),
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

  if (USING_LEGACY_INDICATOR && IS_MAC) {
    return;
  }

  if (!expected) {
    let win = Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
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

    if (
      !USING_LEGACY_INDICATOR &&
      expected.screen &&
      expected.screen.startsWith("Window")
    ) {
      // These tests were originally written to express window sharing by
      // having expected.screen start with "Window". This meant that the
      // legacy indicator is expected to have the "sharingscreen" attribute
      // set to true when sharing a window.
      //
      // The new indicator, however, differentiates between screen, window
      // and browser window sharing. If we're using the new indicator, we
      // update the expectations accordingly. This can be removed once we
      // are able to remove the tests for the legacy indicator.
      expected.screen = null;
      expected.window = true;
    }

    if (!USING_LEGACY_INDICATOR && !SHOW_GLOBAL_MUTE_TOGGLES) {
      expected.video = false;
      expected.audio = false;

      let visible = docElt.getAttribute("visible") == "true";

      if (!expected.screen && !expected.window && !expected.browserwindow) {
        ok(!visible, "Indicator should not be visible in this configuation.");
      } else {
        ok(visible, "Indicator should be visible.");
      }
    }

    for (let item of ["video", "audio", "screen", "window", "browserwindow"]) {
      let expectedValue;

      if (USING_LEGACY_INDICATOR) {
        expectedValue = expected && expected[item] ? "true" : "";
      } else {
        expectedValue = expected && expected[item] ? "true" : null;
      }

      is(
        docElt.getAttribute("sharing" + item),
        expectedValue,
        item + " global indicator attribute as expected"
      );
    }

    ok(!indicator.hasMoreElements(), "only one global indicator window");
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

  let browsingContext = Element.isInstance(browser)
    ? browser.browsingContext
    : browser;

  return BrowserTestUtils.contentTopicObserved(browsingContext, aTopic, aCount);
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

  let browsingContext = Element.isInstance(browser)
    ? browser.browsingContext
    : browser;

  return new Promise(resolve => {
    BrowserTestUtils.sendAsyncMessage(
      browsingContext,
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

function promiseMessage(
  aMessage,
  aAction,
  aCount = 1,
  browser = gBrowser.selectedBrowser
) {
  let startTime = performance.now();
  let promise = ContentTask.spawn(browser, [aMessage, aCount], async function([
    expectedMessage,
    expectedCount,
  ]) {
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
  });
  if (aAction) {
    aAction();
  }
  return promise.then(data => {
    ChromeUtils.addProfilerMarker(
      "promiseMessage",
      { startTime, category: "Test" },
      data
    );
    return data;
  });
}

function promisePopupNotificationShown(aName, aAction, aWindow = window) {
  let startTime = performance.now();
  return new Promise(resolve => {
    // In case the global webrtc indicator has stolen focus (bug 1421724)
    aWindow.focus();

    aWindow.PopupNotifications.panel.addEventListener(
      "popupshown",
      function() {
        ok(
          !!aWindow.PopupNotifications.getNotification(aName),
          aName + " notification shown"
        );
        ok(aWindow.PopupNotifications.isPanelOpen, "notification panel open");
        ok(
          !!aWindow.PopupNotifications.panel.firstElementChild,
          "notification panel populated"
        );

        executeSoon(() => {
          ChromeUtils.addProfilerMarker(
            "promisePopupNotificationShown",
            { startTime, category: "Test" },
            aName
          );
          resolve();
        });
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
  let startTime = performance.now();

  function gatherBrowsingContexts(aBrowsingContext) {
    let list = [aBrowsingContext];

    let children = aBrowsingContext.children;
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
      let devices = {};
      mediaManagerService.mediaCaptureWindowState(
        content,
        hasCamera,
        hasMicrophone,
        hasScreenShare,
        hasWindowShare,
        hasBrowserShare,
        devices,
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

  ChromeUtils.addProfilerMarker("getMediaCaptureState", {
    startTime,
    category: "Test",
  });
  return result;
}

async function stopSharing(
  aType = "camera",
  aShouldKeepSharing = false,
  aFrameBC,
  aWindow = window
) {
  let promiseRecordingEvent = expectObserverCalled(
    "recording-device-events",
    1,
    aFrameBC
  );
  aWindow.gPermissionPanel._identityPermissionBox.click();
  let popup = aWindow.gPermissionPanel._permissionPopup;
  // If the popup gets hidden before being shown, by stray focus/activate
  // events, don't bother failing the test. It's enough to know that we
  // started showing the popup.
  let hiddenEvent = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  let shownEvent = BrowserTestUtils.waitForEvent(popup, "popupshown");
  await Promise.race([hiddenEvent, shownEvent]);
  let doc = aWindow.document;
  let permissions = doc.getElementById("permission-popup-permission-list");
  let cancelButton = permissions.querySelector(
    ".permission-popup-permission-icon." +
      aType +
      "-icon ~ " +
      ".permission-popup-permission-remove-button"
  );
  let observerPromise1 = expectObserverCalled(
    "getUserMedia:revoke",
    1,
    aFrameBC
  );

  // If we are stopping screen sharing and expect to still have another stream,
  // "recording-window-ended" won't be fired.
  let observerPromise2 = null;
  if (!aShouldKeepSharing) {
    observerPromise2 = expectObserverCalled(
      "recording-window-ended",
      1,
      aFrameBC
    );
  }

  cancelButton.click();
  popup.hidePopup();

  await promiseRecordingEvent;
  await observerPromise1;
  await observerPromise2;

  if (!aShouldKeepSharing) {
    await checkNotSharing();
  }
}

function getBrowsingContextForFrame(aBrowsingContext, aFrameId) {
  if (!aFrameId) {
    return aBrowsingContext;
  }

  return SpecialPowers.spawn(aBrowsingContext, [aFrameId], frameId => {
    return content.document.getElementById(frameId).browsingContext;
  });
}

async function getBrowsingContextsAndFrameIdsForSubFrames(
  aBrowsingContext,
  aSubFrames
) {
  let pendingBrowserSubFrames = [
    { bc: aBrowsingContext, subFrames: aSubFrames },
  ];
  let browsingContextsAndFrames = [];
  while (pendingBrowserSubFrames.length) {
    let { bc, subFrames } = pendingBrowserSubFrames.shift();
    for (let id of Object.keys(subFrames)) {
      let subBc = await getBrowsingContextForFrame(bc, id);
      if (subFrames[id].children) {
        pendingBrowserSubFrames.push({
          bc: subBc,
          subFrames: subFrames[id].children,
        });
      }
      if (subFrames[id].noTest) {
        continue;
      }
      let observeBC = subFrames[id].observe ? subBc : undefined;
      browsingContextsAndFrames.push({ bc: subBc, id, observeBC });
    }
  }
  return browsingContextsAndFrames;
}

async function promiseRequestDevice(
  aRequestAudio,
  aRequestVideo,
  aFrameId,
  aType,
  aBrowsingContext,
  aBadDevice = false
) {
  info("requesting devices");
  let bc =
    aBrowsingContext ??
    (await getBrowsingContextForFrame(gBrowser.selectedBrowser, aFrameId));
  return SpecialPowers.spawn(
    bc,
    [{ aRequestAudio, aRequestVideo, aType, aBadDevice }],
    async function(args) {
      let global = content.wrappedJSObject;
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
  aDontFlushObserverVerification,
  aBrowsingContext,
  aBrowsingContextToObserve
) {
  // Check that spurious notifications that occur while closing the
  // stream are handled separately. Tests that use skipObserverVerification
  // should pass true for aDontFlushObserverVerification.
  if (!aDontFlushObserverVerification) {
    await disableObserverVerification();
    await enableObserverVerification();
  }

  // If the observers are listening to other frames, listen for a notification
  // on the right subframe.
  let frameBC =
    aBrowsingContext ??
    (await getBrowsingContextForFrame(
      gBrowser.selectedBrowser.browsingContext,
      aFrameId
    ));

  let observerPromises = [];
  if (!aAlreadyClosed) {
    observerPromises.push(
      expectObserverCalled(
        "recording-device-events",
        1,
        aBrowsingContextToObserve
      )
    );
    observerPromises.push(
      expectObserverCalled(
        "recording-window-ended",
        1,
        aBrowsingContextToObserve
      )
    );
  }

  info("closing the stream");
  await SpecialPowers.spawn(frameBC, [], async function() {
    content.wrappedJSObject.closeStream();
  });

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

function checkDeviceSelectors(aAudio, aVideo, aScreen, aWindow = window) {
  let document = aWindow.document;
  let micSelector = document.getElementById("webRTC-selectMicrophone");
  let micDeck = document.getElementById("webRTC-selectMicrophone-deck");
  let micLabel = document.getElementById("webRTC-selectMicrophone-label");
  if (aAudio) {
    ok(!micSelector.hidden, "microphone selector visible");
    let micSelectorList = document.getElementById(
      "webRTC-selectMicrophone-menulist"
    );
    // If there's only 1 device listed, the deck should show the label instead.
    if (micSelectorList.itemCount == 1) {
      is(micDeck.selectedIndex, "1", "Should be showing the microphone label.");
      is(
        micLabel.value,
        micSelectorList.selectedItem.getAttribute("label"),
        "Label should be showing the lone device label."
      );
    } else {
      is(
        micDeck.selectedIndex,
        "0",
        "Should be showing the microphone menulist."
      );
    }
  } else {
    ok(micSelector.hidden, "microphone selector hidden");
  }

  let cameraSelector = document.getElementById("webRTC-selectCamera");
  let cameraDeck = document.getElementById("webRTC-selectCamera-deck");
  let cameraLabel = document.getElementById("webRTC-selectCamera-label");
  if (aVideo) {
    ok(!cameraSelector.hidden, "camera selector visible");
    let cameraSelectorList = document.getElementById(
      "webRTC-selectCamera-menulist"
    );
    // If there's only 1 device listed, the deck should show the label instead.
    if (cameraSelectorList.itemCount == 1) {
      is(cameraDeck.selectedIndex, "1", "Should be showing the camera label.");
      is(
        cameraLabel.value,
        cameraSelectorList.selectedItem.getAttribute("label"),
        "Label should be showing the lone device label."
      );
    } else {
      is(
        cameraDeck.selectedIndex,
        "0",
        "Should be showing the camera menulist."
      );
    }
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

/**
 * Tests the siteIdentity icons, the permission panel and the global indicator
 * UI state.
 * @param {Object} aExpected - Expected state for the current tab.
 * @param {window} [aWin] - Top level chrome window to test state of.
 * @param {Object} [aExpectedGlobal] - Expected state for all tabs.
 * @param {Object} [aExpectedPerm] - Expected permission states keyed by device
 * type.
 */
async function checkSharingUI(
  aExpected,
  aWin = window,
  aExpectedGlobal = null,
  aExpectedPerm = null
) {
  function isPaused(streamState) {
    if (typeof streamState == "string") {
      return streamState.includes("Paused");
    }
    return streamState == STATE_CAPTURE_DISABLED;
  }

  let doc = aWin.document;
  // First check the icon above the control center (i) icon.
  let permissionBox = doc.getElementById("identity-permission-box");
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

  // Then check the sharing indicators inside the permission popup.
  permissionBox.click();
  let popup = aWin.gPermissionPanel._permissionPopup;
  // If the popup gets hidden before being shown, by stray focus/activate
  // events, don't bother failing the test. It's enough to know that we
  // started showing the popup.
  let hiddenEvent = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  let shownEvent = BrowserTestUtils.waitForEvent(popup, "popupshown");
  await Promise.race([hiddenEvent, shownEvent]);
  let permissions = doc.getElementById("permission-popup-permission-list");
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

    // Extract the expected permission for the device type.
    // Defaults to temporary allow.
    let { state, scope } = aExpectedPerm?.[convertId(id)] || {};
    if (state == null) {
      state = SitePermissions.ALLOW;
    }
    if (scope == null) {
      scope = SitePermissions.SCOPE_TEMPORARY;
    }

    is(
      !!aWin.gPermissionPanel._sharingState.webRTC[id],
      !!expected,
      "sharing state for " + id + " as expected"
    );
    let item = permissions.querySelectorAll(
      ".permission-popup-permission-item-" + id
    );
    let stateLabel = item?.[0]?.querySelector(
      ".permission-popup-permission-state-label"
    );
    let icon = permissions.querySelectorAll(
      ".permission-popup-permission-icon." + id + "-icon"
    );
    if (expected) {
      is(item.length, 1, "should show " + id + " item in permission panel");
      is(
        stateLabel?.textContent,
        SitePermissions.getCurrentStateLabel(state, id, scope),
        "should show correct item label for " + id
      );
      is(icon.length, 1, "should show " + id + " icon in permission panel");
      is(
        icon[0].classList.contains("in-use"),
        expected && !isPaused(expected),
        "icon should have the in-use class, unless paused"
      );
    } else if (!icon.length && !item.length && !stateLabel) {
      ok(true, "should not show " + id + " item in the permission panel");
      ok(true, "should not show " + id + " icon in the permission panel");
      ok(
        true,
        "should not show " + id + " state label in the permission panel"
      );
    } else {
      // This will happen if there are persistent permissions set.
      ok(
        !icon[0].classList.contains("in-use"),
        "if shown, the " + id + " icon should not have the in-use class"
      );
      is(item.length, 1, "should not show more than 1 " + id + " item");
      is(icon.length, 1, "should not show more than 1 " + id + " icon");
    }
  }
  aWin.gPermissionPanel._permissionPopup.hidePopup();
  await TestUtils.waitForCondition(
    () => permissionPopupHidden(aWin),
    "identity popup should be hidden"
  );

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

async function promiseReloadFrame(aFrameId, aBrowsingContext) {
  let loadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    true,
    arg => {
      return true;
    }
  );
  let bc =
    aBrowsingContext ??
    (await getBrowsingContextForFrame(
      gBrowser.selectedBrowser.browsingContext,
      aFrameId
    ));
  await SpecialPowers.spawn(bc, [], async function() {
    content.location.reload();
  });
  return loadedPromise;
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
async function enableObserverVerification(browser = gBrowser.selectedBrowser) {
  // Skip these checks in single process mode as it isn't worth implementing it.
  if (!gMultiProcessBrowser) {
    return;
  }

  gBrowserContextsToObserve = [browser.browsingContext];

  // A list of subframe indicies to also add observers to. This only
  // supports one nested level.
  if (gObserveSubFrames) {
    let bcsAndFrameIds = await getBrowsingContextsAndFrameIdsForSubFrames(
      browser,
      gObserveSubFrames
    );
    for (let { observeBC } of bcsAndFrameIds) {
      if (observeBC) {
        gBrowserContextsToObserve.push(observeBC);
      }
    }
  }

  for (let bc of gBrowserContextsToObserve) {
    await BrowserTestUtils.startObservingTopics(bc, observerTopics);
  }
}

async function disableObserverVerification() {
  if (!gMultiProcessBrowser) {
    return;
  }

  for (let bc of gBrowserContextsToObserve) {
    await BrowserTestUtils.stopObservingTopics(bc, observerTopics).catch(
      reason => {
        ok(false, "Failed " + reason);
      }
    );
  }
}

function permissionPopupHidden(win = window) {
  let popup = win.gPermissionPanel._permissionPopup;
  return !popup || popup.state == "closed";
}

async function runTests(tests, options = {}) {
  let browser = await openNewTestTab(options.relativeURI);

  is(
    PopupNotifications._currentNotifications.length,
    0,
    "should start the test without any prior popup notification"
  );
  ok(
    permissionPopupHidden(),
    "should start the test with the permission panel hidden"
  );

  // Set prefs so that permissions prompts are shown and loopback devices
  // are not used. To test the chrome we want prompts to be shown, and
  // these tests are flakey when using loopback devices (though it would
  // be desirable to make them work with loopback in future). See bug 1643711.
  let prefs = [
    [PREF_PERMISSION_FAKE, true],
    [PREF_AUDIO_LOOPBACK, ""],
    [PREF_VIDEO_LOOPBACK, ""],
    [PREF_FAKE_STREAMS, true],
    [PREF_FOCUS_SOURCE, false],
  ];
  await SpecialPowers.pushPrefEnv({ set: prefs });

  // When the frames are in different processes, add observers to each frame,
  // to ensure that the notifications don't get sent in the wrong process.
  gObserveSubFrames = SpecialPowers.useRemoteSubframes ? options.subFrames : {};

  for (let testCase of tests) {
    let startTime = performance.now();
    info(testCase.desc);
    if (
      !testCase.skipObserverVerification &&
      !options.skipObserverVerification
    ) {
      await enableObserverVerification();
    }
    await testCase.run(browser, options.subFrames);
    if (
      !testCase.skipObserverVerification &&
      !options.skipObserverVerification
    ) {
      await disableObserverVerification();
    }
    if (options.cleanup) {
      await options.cleanup();
    }
    ChromeUtils.addProfilerMarker(
      "browser-test",
      { startTime, category: "Test" },
      testCase.desc
    );
  }

  // Some tests destroy the original tab and leave a new one in its place.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

/**
 * Given a browser from a tab in this window, chooses to share
 * some combination of camera, mic or screen.
 *
 * @param {<xul:browser} browser - The browser to share devices with.
 * @param {boolean} camera - True to share a camera device.
 * @param {boolean} mic - True to share a microphone device.
 * @param {Number} [screenOrWin] - One of either SHARE_WINDOW or SHARE_SCREEN
 *   to share a window or screen. Defaults to neither.
 * @param {boolean} remember - True to persist the permission to the
 *   SitePermissions database as SitePermissions.SCOPE_PERSISTENT. Note that
 *   callers are responsible for clearing this persistent permission.
 * @return {Promise}
 * @resolves {undefined} - Once the sharing is complete.
 */
async function shareDevices(
  browser,
  camera,
  mic,
  screenOrWin = 0,
  remember = false
) {
  if (camera || mic) {
    let promise = promisePopupNotificationShown(
      "webRTC-shareDevices",
      null,
      window
    );

    await promiseRequestDevice(mic, camera, null, null, browser);
    await promise;

    checkDeviceSelectors(mic, camera);
    let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
    let observerPromise2 = expectObserverCalled("recording-device-events");

    let rememberCheck = PopupNotifications.panel.querySelector(
      ".popup-notification-checkbox"
    );
    rememberCheck.checked = remember;

    promise = promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });

    await observerPromise1;
    await observerPromise2;
    await promise;
  }

  if (screenOrWin) {
    let promise = promisePopupNotificationShown(
      "webRTC-shareDevices",
      null,
      window
    );

    await promiseRequestDevice(false, true, null, "screen", browser);
    await promise;

    checkDeviceSelectors(false, false, true, window);

    let document = window.document;

    let menulist = document.getElementById("webRTC-selectWindow-menulist");
    let displayMediaSource;

    if (screenOrWin == SHARE_SCREEN) {
      displayMediaSource = "screen";
    } else if (screenOrWin == SHARE_WINDOW) {
      displayMediaSource = "window";
    } else {
      throw new Error("Got an invalid argument to shareDevices.");
    }

    let menuitem = null;
    for (let i = 0; i < menulist.itemCount; ++i) {
      let current = menulist.getItemAtIndex(i);
      if (current.mediaSource == displayMediaSource) {
        menuitem = current;
        break;
      }
    }

    Assert.ok(menuitem, "Should have found an appropriate display menuitem");
    menuitem.doCommand();

    let notification = window.PopupNotifications.panel.firstElementChild;

    let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
    let observerPromise2 = expectObserverCalled("recording-device-events");
    await promiseMessage(
      "ok",
      () => {
        notification.button.click();
      },
      1,
      browser
    );
    await observerPromise1;
    await observerPromise2;
  }
}
