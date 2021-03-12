/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

SpecialPowers.pushPrefEnv({
  set: [["permissions.delegation.enabled", true]],
});

// This test has been seen timing out locally in non-opt debug builds.
requestLongerTimeout(2);

var gTests = [
  {
    desc: "getUserMedia audio+video",
    run: async function checkAudioVideo(aBrowser, aSubFrames) {
      let { bc: frame1BC, id: frame1ID, observeBC: frame1ObserveBC } = (
        await getBrowsingContextsAndFrameIdsForSubFrames(
          aBrowser.browsingContext,
          aSubFrames
        )
      )[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
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
        frame1ObserveBC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1ObserveBC
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
      await closeStream(false, frame1ID, undefined, frame1BC, frame1ObserveBC);
    },
  },

  {
    desc: "getUserMedia audio+video: stop sharing",
    run: async function checkStopSharing(aBrowser, aSubFrames) {
      let { bc: frame1BC, id: frame1ID, observeBC: frame1ObserveBC } = (
        await getBrowsingContextsAndFrameIdsForSubFrames(
          aBrowser.browsingContext,
          aSubFrames
        )
      )[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1ObserveBC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1ObserveBC
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
      await checkSharingUI({ video: true, audio: true }, undefined, undefined, {
        video: { scope: SitePermissions.SCOPE_PERSISTENT },
        audio: { scope: SitePermissions.SCOPE_PERSISTENT },
      });

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

      await stopSharing("camera", false, frame1ObserveBC);

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
      await closeStream(true, frame1ID, undefined, frame1BC, frame1ObserveBC);
    },
  },

  {
    desc:
      "getUserMedia audio+video: reloading the frame removes all sharing UI",
    run: async function checkReloading(aBrowser, aSubFrames) {
      let { bc: frame1BC, id: frame1ID, observeBC: frame1ObserveBC } = (
        await getBrowsingContextsAndFrameIdsForSubFrames(
          aBrowser.browsingContext,
          aSubFrames
        )
      )[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1ObserveBC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1ObserveBC
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
        expectObserverCalledOnClose(
          "recording-device-stopped",
          1,
          frame1ObserveBC
        ),
        expectObserverCalledOnClose(
          "recording-device-events",
          1,
          frame1ObserveBC
        ),
        expectObserverCalledOnClose(
          "recording-window-ended",
          1,
          frame1ObserveBC
        ),
      ];
      await promiseReloadFrame(frame1ID, frame1BC);
      await Promise.all(promises);

      await enableObserverVerification();

      await checkNotSharing();
    },
  },

  {
    desc: "getUserMedia audio+video: reloading the frame removes prompts",
    run: async function checkReloadingRemovesPrompts(aBrowser, aSubFrames) {
      let { bc: frame1BC, id: frame1ID, observeBC: frame1ObserveBC } = (
        await getBrowsingContextsAndFrameIdsForSubFrames(
          aBrowser.browsingContext,
          aSubFrames
        )
      )[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      info("reloading the frame");
      promise = expectObserverCalledOnClose(
        "recording-window-ended",
        1,
        frame1ObserveBC
      );
      await promiseReloadFrame(frame1ID, frame1BC);
      await promise;
      await promiseNoPopupNotification("webRTC-shareDevices");

      await checkNotSharing();
    },
  },

  {
    desc:
      "getUserMedia audio+video: with two frames sharing at the same time, sharing UI shows all shared devices",
    run: async function checkFrameOverridingSharingUI(aBrowser, aSubFrames) {
      // This tests an edge case discovered in bug 1440356 that works like this
      // - Share audio and video in iframe 1.
      // - Share only video in iframe 2.
      // The WebRTC UI should still show both video and audio indicators.

      let bcsAndFrameIds = await getBrowsingContextsAndFrameIdsForSubFrames(
        aBrowser.browsingContext,
        aSubFrames
      );
      let {
        bc: frame1BC,
        id: frame1ID,
        observeBC: frame1ObserveBC,
      } = bcsAndFrameIds[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1ObserveBC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1ObserveBC
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
      let {
        bc: frame2BC,
        id: frame2ID,
        observeBC: frame2ObserveBC,
      } = bcsAndFrameIds[1];

      observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame2ObserveBC
      );
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, frame2ID, undefined, frame2BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(false, true);

      observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame2ObserveBC
      );
      observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame2ObserveBC
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
        frame2ObserveBC
      );
      promise = expectObserverCalledOnClose(
        "recording-device-events",
        1,
        frame2ObserveBC
      );
      await promiseReloadFrame(frame2ID, frame2BC);
      await promise;

      await observerPromise;
      await checkSharingUI({ video: true, audio: true });

      await closeStream(false, frame1ID, undefined, frame1BC, frame1ObserveBC);
      await checkNotSharing();
    },
  },

  {
    desc: "getUserMedia audio+video: reloading a frame updates the sharing UI",
    run: async function checkUpdateWhenReloading(aBrowser, aSubFrames) {
      // We'll share only the cam in the first frame, then share both in the
      // second frame, then reload the second frame. After each step, we'll check
      // the UI is in the correct state.
      let bcsAndFrameIds = await getBrowsingContextsAndFrameIdsForSubFrames(
        aBrowser.browsingContext,
        aSubFrames
      );
      let {
        bc: frame1BC,
        id: frame1ID,
        observeBC: frame1ObserveBC,
      } = bcsAndFrameIds[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, frame1ID, undefined, frame1BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(false, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1ObserveBC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1ObserveBC
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

      let {
        bc: frame2BC,
        id: frame2ID,
        observeBC: frame2ObserveBC,
      } = bcsAndFrameIds[1];

      observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame2ObserveBC
      );
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame2ID, undefined, frame2BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame2ObserveBC
      );
      observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame2ObserveBC
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
        frame2ObserveBC
      );
      observerPromise2 = expectObserverCalledOnClose(
        "recording-window-ended",
        1,
        frame2ObserveBC
      );
      await promiseReloadFrame(frame2ID, frame2BC);
      await observerPromise1;
      await observerPromise2;

      await checkSharingUI({ video: true, audio: false });

      await closeStream(false, frame1ID, undefined, frame1BC, frame1ObserveBC);
      await checkNotSharing();
    },
  },

  {
    desc:
      "getUserMedia audio+video: reloading the top level page removes all sharing UI",
    run: async function checkReloading(aBrowser, aSubFrames) {
      let { bc: frame1BC, id: frame1ID, observeBC: frame1ObserveBC } = (
        await getBrowsingContextsAndFrameIdsForSubFrames(
          aBrowser.browsingContext,
          aSubFrames
        )
      )[0];

      let observerPromise = expectObserverCalled(
        "getUserMedia:request",
        1,
        frame1ObserveBC
      );
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow",
        1,
        frame1ObserveBC
      );
      let observerPromise2 = expectObserverCalled(
        "recording-device-events",
        1,
        frame1ObserveBC
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
    run: async function checkFrameIndicatorClosedUI(aBrowser, aSubFrames) {
      // This tests a case where the indicator didn't close when audio/video is
      // shared in two subframes and then the tabs are closed.

      let tabsToRemove = [gBrowser.selectedTab];

      for (let t = 0; t < 2; t++) {
        let { bc: frame1BC, id: frame1ID, observeBC: frame1ObserveBC } = (
          await getBrowsingContextsAndFrameIdsForSubFrames(
            gBrowser.selectedBrowser.browsingContext,
            aSubFrames
          )
        )[0];

        let observerPromise = expectObserverCalled(
          "getUserMedia:request",
          1,
          frame1ObserveBC
        );
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(true, true, frame1ID, undefined, frame1BC);
        await promise;
        await observerPromise;
        checkDeviceSelectors(true, true);

        // During the second pass, the indicator is already open.
        let indicator = t == 0 ? promiseIndicatorWindow() : Promise.resolve();

        let observerPromise1 = expectObserverCalled(
          "getUserMedia:response:allow",
          1,
          frame1ObserveBC
        );
        let observerPromise2 = expectObserverCalled(
          "recording-device-events",
          1,
          frame1ObserveBC
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
  await runTests(gTests, {
    relativeURI: "get_user_media_in_frame.html",
    subFrames: { frame1: {}, frame2: {} },
  });
});

add_task(async function test_outofprocess() {
  const origin1 = encodeURI("https://test1.example.org");
  const origin2 = encodeURI("https://www.mozilla.org:443");
  const query = `origin=${origin1}&origin=${origin2}`;
  const observe = SpecialPowers.useRemoteSubframes;
  await runTests(gTests, {
    relativeURI: `get_user_media_in_frame.html?${query}`,
    subFrames: { frame1: { observe }, frame2: { observe } },
  });
});

add_task(async function test_inprocess_in_outofprocess() {
  const oopOrigin = encodeURI("https://www.mozilla.org");
  const sameOrigin = encodeURI("https://example.com");
  const query = `origin=${oopOrigin}&nested=${sameOrigin}&nested=${sameOrigin}`;
  await runTests(gTests, {
    relativeURI: `get_user_media_in_frame.html?${query}`,
    subFrames: {
      frame1: {
        noTest: true,
        children: { frame1: {}, frame2: {} },
      },
    },
  });
});
