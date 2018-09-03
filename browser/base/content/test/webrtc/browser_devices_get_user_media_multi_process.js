/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [

{
  desc: "getUserMedia audio in a first process + video in a second process",
  run: async function checkMultiProcess() {
    // The main purpose of this test is to ensure webrtc sharing indicators
    // work with multiple content processes, but it makes sense to run this
    // test without e10s too to ensure using webrtc devices in two different
    // tabs is handled correctly.

    // Request audio in the first tab.
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(true);
    await promise;
    await expectObserverCalled("getUserMedia:request");

    checkDeviceSelectors(true);

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {audio: true},
                     "expected microphone to be shared");

    await indicator;
    await checkSharingUI({audio: true});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(!webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator hidden");
    ok(webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator shown");
    is(webrtcUI.getActiveStreams(false, true).length, 1, "1 active audio stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 1, "1 active stream");

    await expectNoObserverCalled();

    // If we have reached the max process count already, increase it to ensure
    // our new tab can have its own content process.
    let childCount = Services.ppmm.childCount;
    let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
    // The first check is because if we are on a branch where e10s-multi is
    // disabled, we want to keep testing e10s with a single content process.
    // The + 1 is because ppmm.childCount also counts the chrome process
    // (which also runs process scripts).
    if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
      await SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount",
                                                childCount]]});
    }

    // Open a new tab with a different hostname.
    let url = gBrowser.currentURI.spec.replace("https://example.com/",
                                               "http://127.0.0.1:8888/");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    tab.linkedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

    // Request video.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true);
    await promise;
    await expectObserverCalled("getUserMedia:request");

    checkDeviceSelectors(false, true);

    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");

    await checkSharingUI({video: true}, window, {audio: true, video: true});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator shown");
    ok(webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator shown");
    is(webrtcUI.getActiveStreams(false, true).length, 1, "1 active audio stream");
    is(webrtcUI.getActiveStreams(true).length, 1, "1 active video stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 2, "2 active streams");

    info("removing the second tab");

    BrowserTestUtils.removeTab(tab);

    // Check that we still show the sharing indicators for the first tab's stream.
    await Promise.all([
      TestUtils.waitForCondition(() => !webrtcUI.showCameraIndicator),
      TestUtils.waitForCondition(() => webrtcUI.getActiveStreams(true, true, true).length == 1),
    ]);

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(!webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator hidden");
    ok(webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator shown");
    is(webrtcUI.getActiveStreams(false, true).length, 1, "1 active audio stream");

    await checkSharingUI({audio: true});

    // When both tabs use the same content process, the frame script for the
    // first tab receives observer notifications for things happening in the
    // second tab, so let's clear the observer call counts before we cleanup
    // in the first tab.
    await ignoreObserversCalled();

    // Close the first tab's stream and verify that all indicators are removed.
    await closeStream();

    ok(!webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator hidden");
    is(webrtcUI.getActiveStreams(true, true, true).length, 0, "0 active streams");
  },
},

{
  desc: "getUserMedia camera in a first process + camera in a second process",
  run: async function checkMultiProcessCamera() {

    // Request camera in the first tab.
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true);
    await promise;
    await expectObserverCalled("getUserMedia:request");

    checkDeviceSelectors(false, true);

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {video: true},
                     "expected camera to be shared");

    await indicator;
    await checkSharingUI({video: true});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator shown");
    ok(!webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator hidden");
    is(webrtcUI.getActiveStreams(true).length, 1, "1 active camera stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 1, "1 active stream");

    await expectNoObserverCalled();

    // If we have reached the max process count already, increase it to ensure
    // our new tab can have its own content process.
    let childCount = Services.ppmm.childCount;
    let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
    // The first check is because if we are on a branch where e10s-multi is
    // disabled, we want to keep testing e10s with a single content process.
    // The + 1 is because ppmm.childCount also counts the chrome process
    // (which also runs process scripts).
    if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
      await SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount",
                                                childCount]]});
    }

    // Open a new tab with a different hostname.
    let url = gBrowser.currentURI.spec.replace("https://example.com/",
                                               "http://127.0.0.1:8888/");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    tab.linkedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

    // Request camera in the second tab.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true);
    await promise;
    await expectObserverCalled("getUserMedia:request");

    checkDeviceSelectors(false, true);

    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");

    await checkSharingUI({video: true}, window, {video: true});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator shown");
    ok(!webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator hidden");
    is(webrtcUI.getActiveStreams(true).length, 2, "2 active camera streams");
    is(webrtcUI.getActiveStreams(true, true, true).length, 2, "2 active streams");

    info("removing the second tab");
    BrowserTestUtils.removeTab(tab);

    // Check that we still show the sharing indicators for the first tab's stream.
    await Promise.all([
      TestUtils.waitForCondition(() => webrtcUI.showCameraIndicator),
      TestUtils.waitForCondition(() => webrtcUI.getActiveStreams(true).length == 1),
    ]);
    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator shown");
    ok(!webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator hidden");
    is(webrtcUI.getActiveStreams(true, true, true).length, 1, "1 active stream");

    await checkSharingUI({video: true});

    // When both tabs use the same content process, the frame script for the
    // first tab receives observer notifications for things happening in the
    // second tab, so let's clear the observer call counts before we cleanup
    // in the first tab.
    await ignoreObserversCalled();

    // Close the first tab's stream and verify that all indicators are removed.
    await closeStream();

    ok(!webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator hidden");
    is(webrtcUI.getActiveStreams(true, true, true).length, 0, "0 active streams");
  },
},

{
  desc: "getUserMedia screen sharing in a first process + screen sharing in a second process",
  run: async function checkMultiProcessScreen() {

    // Request screen sharing in the first tab.
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);

    // Select the first screen so that we can have a stream.
    document.getElementById("webRTC-selectWindow-menulist").getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");

    await indicator;
    await checkSharingUI({screen: "Screen"});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showScreenSharingIndicator, "webrtcUI wants the screen sharing indicator shown");
    ok(!webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator hidden");
    ok(!webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator hidden");
    is(webrtcUI.getActiveStreams(false, false, true).length, 1, "1 active screen sharing stream");
    is(webrtcUI.getActiveStreams(true, true, true).length, 1, "1 active stream");

    await expectNoObserverCalled();

    // If we have reached the max process count already, increase it to ensure
    // our new tab can have its own content process.
    let childCount = Services.ppmm.childCount;
    let maxContentProcess = Services.prefs.getIntPref("dom.ipc.processCount");
    // The first check is because if we are on a branch where e10s-multi is
    // disabled, we want to keep testing e10s with a single content process.
    // The + 1 is because ppmm.childCount also counts the chrome process
    // (which also runs process scripts).
    if (maxContentProcess > 1 && childCount == maxContentProcess + 1) {
      await SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount",
                                                childCount]]});
    }

    // Open a new tab with a different hostname.
    let url = gBrowser.currentURI.spec.replace("https://example.com/",
                                               "https://example.com/");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    tab.linkedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

    // Request screen sharing in the second tab.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);

    // Select the first screen so that we can have a stream.
    document.getElementById("webRTC-selectWindow-menulist").getItemAtIndex(2).doCommand();

    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstElementChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");

    await checkSharingUI({screen: "Screen"}, window, {screen: "Screen"});

    ok(webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator shown");
    ok(webrtcUI.showScreenSharingIndicator, "webrtcUI wants the screen sharing indicator shown");
    ok(!webrtcUI.showCameraIndicator, "webrtcUI wants the camera indicator hidden");
    ok(!webrtcUI.showMicrophoneIndicator, "webrtcUI wants the mic indicator hidden");
    is(webrtcUI.getActiveStreams(false, false, true).length, 2, "2 active desktop sharing streams");
    is(webrtcUI.getActiveStreams(true, true, true).length, 2, "2 active streams");

    info("removing the second tab");
    BrowserTestUtils.removeTab(tab);

    // When both tabs use the same content process, the frame script for the
    // first tab receives observer notifications for things happening in the
    // second tab, so let's clear the observer call counts before we cleanup
    // in the first tab.
    await Promise.all([
      TestUtils.waitForCondition(() => webrtcUI.getActiveStreams(true, true, true).length == 1),
      ignoreObserversCalled(),
    ]);

    // Close the first tab's stream and verify that all indicators are removed.
    await closeStream();

    ok(!webrtcUI.showGlobalIndicator, "webrtcUI wants the global indicator hidden");
    is(webrtcUI.getActiveStreams(true, true, true).length, 0, "0 active streams");
  },
},

];

add_task(async function test() {
  await runTests(gTests);
});
