/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [

{
  desc: "getUserMedia: tearing-off a tab",
  run: async function checkAudioVideoWhileLiveTracksExist_TearingOff() {
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

    gBrowser.selectedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

    info("request audio+video and check if there is no prompt");
    await promiseRequestDevice(true, true, null, null, win.gBrowser.selectedBrowser);
    await promiseObserverCalled("getUserMedia:request");
    let promises = [promiseNoPopupNotification("webRTC-shareDevices"),
                    promiseObserverCalled("getUserMedia:response:allow"),
                    promiseObserverCalled("recording-device-events")];
    await Promise.all(promises);

    promises = [promiseObserverCalled("recording-device-events"),
                promiseObserverCalled("recording-window-ended")];
    await BrowserTestUtils.closeWindow(win);
    await Promise.all(promises);

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
