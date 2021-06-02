/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [
  {
    desc: "getUserMedia: tearing-off a tab",
    skipObserverVerification: true,
    run: async function checkAudioVideoWhileLiveTracksExist_TearingOff() {
      await enableObserverVerification();

      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      let observerPromise = expectObserverCalled("getUserMedia:request");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(["microphone", "camera"]);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      // Don't listen to observer notifications in the tab any more, as
      // they will need to be switched to the new window.
      await disableObserverVerification();

      info("tearing off the tab");
      let win = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
      await whenDelayedStartupFinished(win);
      await SimpleTest.promiseFocus(win);
      await checkSharingUI({ audio: true, video: true }, win);

      await enableObserverVerification(win.gBrowser.selectedBrowser);

      info("request audio+video and check if there is no prompt");
      let observerPromises = [
        expectObserverCalled(
          "getUserMedia:request",
          1,
          win.gBrowser.selectedBrowser
        ),
        expectObserverCalled(
          "getUserMedia:response:allow",
          1,
          win.gBrowser.selectedBrowser
        ),
        expectObserverCalled(
          "recording-device-events",
          1,
          win.gBrowser.selectedBrowser
        ),
      ];
      await promiseRequestDevice(
        true,
        true,
        null,
        null,
        win.gBrowser.selectedBrowser
      );
      await Promise.all(observerPromises);

      await disableObserverVerification();

      observerPromises = [
        expectObserverCalledOnClose(
          "recording-device-events",
          1,
          win.gBrowser.selectedBrowser
        ),
        expectObserverCalledOnClose(
          "recording-window-ended",
          1,
          win.gBrowser.selectedBrowser
        ),
      ];

      await BrowserTestUtils.closeWindow(win);
      await Promise.all(observerPromises);

      await checkNotSharing();
    },
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({ set: [["dom.ipc.processCount", 1]] });

  // An empty tab where we can load the content script without leaving it
  // behind at the end of the test.
  BrowserTestUtils.addTab(gBrowser);

  await runTests(gTests);
});
