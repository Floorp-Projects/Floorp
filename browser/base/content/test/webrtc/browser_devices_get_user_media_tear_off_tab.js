/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [
  {
    desc: "getUserMedia: tearing-off a tab keeps sharing indicators",
    skipObserverVerification: true,
    run: async function checkTearingOff() {
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
      await checkSharingUI({ audio: true, video: true }, win);

      await enableObserverVerification(win.gBrowser.selectedBrowser);

      // Clicking the global sharing indicator should open the control center in
      // the second window.
      ok(permissionPopupHidden(win), "control center should be hidden");
      let activeStreams = webrtcUI.getActiveStreams(true, false, false);
      webrtcUI.showSharingDoorhanger(activeStreams[0]);
      // If the popup gets hidden before being shown, by stray focus/activate
      // events, don't bother failing the test. It's enough to know that we
      // started showing the popup.
      let popup = win.gPermissionPanel._permissionPopup;
      let hiddenEvent = BrowserTestUtils.waitForEvent(popup, "popuphidden");
      let shownEvent = BrowserTestUtils.waitForEvent(popup, "popupshown");
      let ev = await Promise.race([hiddenEvent, shownEvent]);
      ok(ev.type, "Tried to show popup");
      win.gPermissionPanel._permissionPopup.hidePopup();

      ok(
        permissionPopupHidden(window),
        "control center should be hidden in the first window"
      );

      await disableObserverVerification();

      // Closing the new window should remove all sharing indicators.
      let promises = [
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
      await Promise.all(promises);
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
