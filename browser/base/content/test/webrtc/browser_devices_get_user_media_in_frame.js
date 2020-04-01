/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

SpecialPowers.pushPrefEnv({
  set: [
    ["dom.security.featurePolicy.enabled", true],
    ["permissions.delegation.enabled", true],
  ],
});

let gShouldObserveSubframes = false;

var gTests = [
  {
    desc: "getUserMedia audio+video",
    run: async function checkAudioVideo() {
      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(true, true);
      is(
        PopupNotifications.panel.firstElementChild.getAttribute("popupid"),
        "webRTC-shareDevices",
        "panel using devices icon"
      );

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1BC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1BC
      );
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
      await checkSharingUI({ audio: true, video: true });
      await closeStream(false, "frame1");
    },
  },

  {
    desc: "getUserMedia audio+video: stop sharing",
    run: async function checkStopSharing() {
      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1BC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1BC
      );
      await promiseMessage("ok", () => {
        activateSecondaryAction(kActionAlways);
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

      let uri = Services.io.newURI("https://example.com/");
      is(
        PermissionTestUtils.testExactPermission(uri, "microphone"),
        Services.perms.ALLOW_ACTION,
        "microphone persistently allowed"
      );
      is(
        PermissionTestUtils.testExactPermission(uri, "camera"),
        Services.perms.ALLOW_ACTION,
        "camera persistently allowed"
      );

      await stopSharing("camera", false, frame1BC);

      // The persistent permissions for the frame should have been removed.
      is(
        PermissionTestUtils.testExactPermission(uri, "microphone"),
        Services.perms.UNKNOWN_ACTION,
        "microphone not persistently allowed"
      );
      is(
        PermissionTestUtils.testExactPermission(uri, "camera"),
        Services.perms.UNKNOWN_ACTION,
        "camera not persistently allowed"
      );

      // the stream is already closed, but this will do some cleanup anyway
      await closeStream(true, "frame1");
    },
  },

  {
    desc:
      "getUserMedia audio+video: reloading the frame removes all sharing UI",
    run: async function checkReloading() {
      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1BC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1BC
      );
      let indicator = promiseIndicatorWindow();
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

      // Disable while loading a new page
      await disableObserverVerification();

      info("reloading the frame");
      let promises = [
        expectObserverCalledOnClose("recording-device-stopped", 1, frame1BC),
        expectObserverCalledOnClose("recording-device-events", 1, frame1BC),
        expectObserverCalledOnClose("recording-window-ended", 1, frame1BC),
      ];
      await promiseReloadFrame("frame1");
      await Promise.all(promises);

      await enableObserverVerification();

      await checkNotSharing();
    },
  },

  {
    desc: "getUserMedia audio+video: reloading the frame removes prompts",
    run: async function checkReloadingRemovesPrompts() {
      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      info("reloading the frame");
      promise = expectObserverCalledOnClose(
        "recording-window-ended",
        1,
        frame1BC
      );
      await promiseReloadFrame("frame1");
      await promise;
      await promiseNoPopupNotification("webRTC-shareDevices");

      await checkNotSharing();
    },
  },

  {
    desc:
      "getUserMedia audio+video: with two frames sharing at the same time, sharing UI shows all shared devices",
    run: async function checkFrameOverridingSharingUI() {
      // This tests an edge case discovered in bug 1440356 that works like this
      // - Share audio and video in iframe 1.
      // - Share only video in iframe 2.
      // The WebRTC UI should still show both video and audio indicators.

      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1BC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1BC
      );
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

      // Check that requesting a new device from a different frame
      // doesn't override sharing UI.
      let frame2BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[1]
        : undefined;

      observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame2BC
      );
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, "frame2");
      await promise;
      await observerPromise;
      checkDeviceSelectors(false, true);

      observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame2BC
      );
      observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame2BC
      );

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

      await checkSharingUI({ video: true, audio: true });

      // Check that ending the stream with the other frame
      // doesn't override sharing UI.

      observerPromise = expectObserverCalledOnClose(
        "recording-window-ended",
        1,
        frame2BC
      );
      promise = expectObserverCalledOnClose(
        "recording-device-events",
        1,
        frame2BC
      );
      await promiseReloadFrame("frame2");
      await promise;

      await observerPromise;
      await checkSharingUI({ video: true, audio: true });

      await closeStream(false, "frame1");
      await checkNotSharing();
    },
  },

  {
    desc: "getUserMedia audio+video: reloading a frame updates the sharing UI",
    run: async function checkUpdateWhenReloading() {
      // We'll share only the cam in the first frame, then share both in the
      // second frame, then reload the second frame. After each step, we'll check
      // the UI is in the correct state.
      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(false, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1BC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1BC
      );
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: false });

      let frame2BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[1]
        : undefined;

      observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame2BC
      );
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame2");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame2BC
      );
      observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame2BC
      );
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

      await checkSharingUI({ video: true, audio: true });

      info("reloading the second frame");

      observerPromise1 = expectObserverCalledOnClose(
        "recording-device-events",
        1,
        frame2BC
      );
      observerPromise2 = expectObserverCalledOnClose(
        "recording-window-ended",
        1,
        frame2BC
      );
      await promiseReloadFrame("frame2");
      await observerPromise1;
      await observerPromise2;

      await checkSharingUI({ video: true, audio: false });

      await closeStream(false, "frame1");
      await checkNotSharing();
    },
  },

  {
    desc:
      "getUserMedia audio+video: reloading the top level page removes all sharing UI",
    run: async function checkReloading() {
      let frame1BC = gShouldObserveSubframes
        ? gBrowser.selectedBrowser.browsingContext.children[0]
        : undefined;

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1BC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1BC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1BC
      );
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

      await reloadAndAssertClosedStreams();
    },
  },

  {
    desc:
      "getUserMedia audio+video: closing a window with two frames sharing at the same time, closes the indicator",
    skipObserverVerification: true,
    run: async function checkFrameIndicatorClosedUI() {
      // This tests a case where the indicator didn't close when audio/video is
      // shared in two subframes and then the tabs are closed.

      let tabsToRemove = [gBrowser.selectedTab];

      for (let t = 0; t < 2; t++) {
        let frame1BC = gShouldObserveSubframes
          ? gBrowser.selectedBrowser.browsingContext.children[0]
          : undefined;

        let observerPromise = expectObserverCalled(
          "getUserMedia:request",
          1,
          frame1BC
        );
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(true, true, "frame1");
        await promise;
        await observerPromise;
        checkDeviceSelectors(true, true);

        // During the second pass, the indicator is already open.
        let indicator = t == 0 ? promiseIndicatorWindow() : Promise.resolve();

        let observerPromise1 = expectObserverCalled(
          "getUserMedia:response:allow",
          1,
          frame1BC
        );
        let observerPromise2 = expectObserverCalled(
          "recording-device-events",
          1,
          frame1BC
        );
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

        // The first time around, open another tab with the same uri.
        // The second time, just open a normal test tab.
        let uri = t == 0 ? gBrowser.selectedBrowser.currentURI.spec : undefined;
        tabsToRemove.push(
          await BrowserTestUtils.openNewForegroundTab(gBrowser, uri)
        );
      }

      BrowserTestUtils.removeTab(tabsToRemove[0]);
      BrowserTestUtils.removeTab(tabsToRemove[1]);

      await checkNotSharing();
    },
  },
];

add_task(async function test_inprocess() {
  await runTests(gTests, { relativeURI: "get_user_media_in_frame.html" });
});

add_task(async function test_outofprocess() {
  // When the frames are in different processes, add observers to each frame,
  // to ensure that the notifications don't get sent in the wrong process.
  gShouldObserveSubframes = SpecialPowers.useRemoteSubframes;

  let observeSubFrameIds = gShouldObserveSubframes ? ["frame1", "frame2"] : [];
  await runTests(gTests, {
    relativeURI: "get_user_media_in_oop_frame.html",
    observeSubFrameIds,
  });
});
