/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [

{
  desc: "getUserMedia: tearing-off a tab keeps sharing indicators",
  run: async function checkTearingOff() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(true, true);
    await promise;
    await expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(true, true);

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {audio: true, video: true},
                     "expected camera and microphone to be shared");

    await indicator;
    await checkSharingUI({video: true, audio: true});

    info("tearing off the tab");
    let win = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
    await whenDelayedStartupFinished(win);
    await checkSharingUI({audio: true, video: true}, win);

    // Clicking the global sharing indicator should open the control center in
    // the second window.
    ok(win.gIdentityHandler._identityPopup.hidden, "control center should be hidden");
    let activeStreams = webrtcUI.getActiveStreams(true, false, false);
    webrtcUI.showSharingDoorhanger(activeStreams[0], "Devices");
    ok(!win.gIdentityHandler._identityPopup.hidden,
       "control center should be open in the second window");
    ok(gIdentityHandler._identityPopup.hidden,
       "control center should be hidden in the first window");
    win.gIdentityHandler._identityPopup.hidden = true;

    // Closing the new window should remove all sharing indicators.
    // We need to load the content script in the first window so that we can
    // catch the notifications fired globally when closing the second window.
    gBrowser.selectedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

    let promises = [promiseObserverCalled("recording-device-events"),
                    promiseObserverCalled("recording-window-ended")];
    await BrowserTestUtils.closeWindow(win);
    await Promise.all(promises);

    await expectNoObserverCalled();
    await checkNotSharing();
  }
}

];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount", 1]]});

  // An empty tab where we can load the content script without leaving it
  // behind at the end of the test.
  BrowserTestUtils.addTab(gBrowser);

  await runTests(gTests);
});
