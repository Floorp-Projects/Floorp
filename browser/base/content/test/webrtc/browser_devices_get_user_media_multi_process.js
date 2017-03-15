/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

var gTests = [

{
  desc: "getUserMedia audio in a first process + video in a second process",
  run: function* checkMultiProcess() {
    // The main purpose of this test is to ensure webrtc sharing indicators
    // work with multiple content processes, but it makes sense to run this
    // test without e10s too to ensure using webrtc devices in two different
    // tabs is handled correctly.

    // Request audio in the first tab.
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    checkDeviceSelectors(true);

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {audio: true},
                     "expected microphone to be shared");

    yield indicator;
    yield checkSharingUI({audio: true});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(!webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator hidden");
    ok(webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator shown");
    is(webrtcUI.getActiveStreams(false, true).length, 1, "1 active audio stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 1, "1 active stream");

    yield expectNoObserverCalled();

    // If we have reached the max process count already, increase it to ensure
    // our new tab can have its own content process.
    var ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                 .getService(Ci.nsIMessageBroadcaster);
    ppmm.QueryInterface(Ci.nsIProcessScriptLoader);
    let childCount = ppmm.childCount;
    let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
    // The first check is because if we are on a branch where e10s-multi is
    // disabled, we want to keep testing e10s with a single content process.
    // The + 1 is because ppmm.childCount also counts the chrome process
    // (which also runs process scripts).
    if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
      yield SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount",
                                                childCount]]});
    }

    // Open a new tab with a different hostname.
    let url = gBrowser.currentURI.spec.replace("https://example.com/",
                                               "http://127.0.0.1:8888/");
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    tab.linkedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

    // Request video.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true);
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    checkDeviceSelectors(false, true);

    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");

    yield checkSharingUI({video: true}, window, {audio: true, video: true});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator shown");
    ok(webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator shown");
    is(webrtcUI.getActiveStreams(false, true).length, 1, "1 active audio stream");
    is(webrtcUI.getActiveStreams(true).length, 1, "1 active video stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 2, "2 active streams");

    info("removing the second tab");
    yield BrowserTestUtils.removeTab(tab);

    // Check that we still show the sharing indicators for the first tab's stream.
    yield promiseWaitForCondition(() => !webrtcUI.showCameraIndicator);
    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(!webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator hidden");
    ok(webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator shown");
    is(webrtcUI.getActiveStreams(false, true).length, 1, "1 active audio stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 1, "1 active stream");

    yield checkSharingUI({audio: true});

    // When both tabs use the same content process, the frame script for the
    // first tab receives observer notifications for things happening in the
    // second tab, so let's clear the observer call counts before we cleanup
    // in the first tab.
    yield ignoreObserversCalled();

    // Close the first tab's stream and verify that all indicators are removed.
    yield closeStream();

    ok(!webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator hidden");
    is(webrtcUI.getActiveStreams(true, true, true).length, 0, "0 active streams");
  }
},

];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  browser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  browser.addEventListener("load", function() {
    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");
    ok(gIdentityHandler._identityPopup.hidden,
       "should start the test with the control center hidden");

    Task.spawn(function* () {
      yield SpecialPowers.pushPrefEnv({"set": [[PREF_PERMISSION_FAKE, true]]});

      for (let testCase of gTests) {
        info(testCase.desc);
        yield testCase.run();
      }
    }).then(finish, ex => {
     Cu.reportError(ex);
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
  }, {capture: true, once: true});
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "https://example.com/");
  content.location = rootDir + "get_user_media.html";
}
