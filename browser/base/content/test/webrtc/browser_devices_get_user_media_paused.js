/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function setTrackEnabled(audio, video) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ audio, video }],
    function(args) {
      let stream = content.wrappedJSObject.gStreams[0];
      if (args.audio != null) {
        stream.getAudioTracks()[0].enabled = args.audio;
      }
      if (args.video != null) {
        stream.getVideoTracks()[0].enabled = args.video;
      }
    }
  );
}

function cloneTracks(audio, video) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ audio, video }],
    function(args) {
      if (!content.wrappedJSObject.gClones) {
        content.wrappedJSObject.gClones = [];
      }
      let clones = content.wrappedJSObject.gClones;
      let stream = content.wrappedJSObject.gStreams[0];
      if (args.audio != null) {
        clones.push(stream.getAudioTracks()[0].clone());
      }
      if (args.video != null) {
        clones.push(stream.getVideoTracks()[0].clone());
      }
    }
  );
}

function stopClonedTracks(audio, video) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ audio, video }],
    function(args) {
      let clones = content.wrappedJSObject.gClones || [];
      if (args.audio != null) {
        clones.filter(t => t.kind == "audio").forEach(t => t.stop());
      }
      if (args.video != null) {
        clones.filter(t => t.kind == "video").forEach(t => t.stop());
      }
      let liveClones = clones.filter(t => t.readyState == "live");
      if (!liveClones.length) {
        delete content.wrappedJSObject.gClones;
      } else {
        content.wrappedJSObject.gClones = liveClones;
      }
    }
  );
}

var gTests = [
  {
    desc:
      "getUserMedia audio+video: disabling the stream shows the paused indicator",
    run: async function checkPaused() {
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

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
      await checkSharingUI({
        video: STATE_CAPTURE_ENABLED,
        audio: STATE_CAPTURE_ENABLED,
      });

      // Disable both audio and video.
      observerPromise = expectObserverCalled("recording-device-events", 2);
      await setTrackEnabled(false, false);

      // It sometimes takes a bit longer before the change propagates to the UI,
      // wait for it to avoid intermittents.
      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
          STATE_CAPTURE_DISABLED,
        "video should be disabled"
      );

      await observerPromise;

      // The identity UI should show both as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_DISABLED,
      });

      // Enable only audio again.
      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(true);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.microphone ==
          STATE_CAPTURE_ENABLED,
        "audio should be enabled"
      );

      await observerPromise;

      // The identity UI should show only video as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_ENABLED,
      });

      // Enable video again.
      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(null, true);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
          STATE_CAPTURE_ENABLED,
        "video should be enabled"
      );

      await observerPromise;

      // Both streams should show as running.
      await checkSharingUI({
        video: STATE_CAPTURE_ENABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      await closeStream();
    },
  },

  {
    desc:
      "getUserMedia audio+video: disabling the original tracks and stopping enabled clones shows the paused indicator",
    run: async function checkPausedAfterCloneStop() {
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

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
      await checkSharingUI({
        video: STATE_CAPTURE_ENABLED,
        audio: STATE_CAPTURE_ENABLED,
      });

      // Clone audio and video, their state will be enabled
      await cloneTracks(true, true);

      // Disable both audio and video.
      await setTrackEnabled(false, false);

      observerPromise = expectObserverCalled("recording-device-events", 2);

      // Stop the clones. This should disable the sharing indicators.
      await stopClonedTracks(true, true);

      // It sometimes takes a bit longer before the change propagates to the UI,
      // wait for it to avoid intermittents.
      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
            STATE_CAPTURE_DISABLED &&
          window.gIdentityHandler._sharingState.webRTC.microphone ==
            STATE_CAPTURE_DISABLED,
        "video and audio should be disabled"
      );

      await observerPromise;

      // The identity UI should show both as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_DISABLED,
      });

      // Enable only audio again.
      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(true);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.microphone ==
          STATE_CAPTURE_ENABLED,
        "audio should be enabled"
      );

      await observerPromise;

      // The identity UI should show only video as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_ENABLED,
      });

      // Enable video again.
      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(null, true);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
          STATE_CAPTURE_ENABLED,
        "video should be enabled"
      );

      await observerPromise;

      // Both streams should show as running.
      await checkSharingUI({
        video: STATE_CAPTURE_ENABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      await closeStream();
    },
  },

  {
    desc:
      "getUserMedia screen: disabling the stream shows the paused indicator",
    run: async function checkScreenPaused() {
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);
      let notification = PopupNotifications.panel.firstElementChild;
      let iconclass = notification.getAttribute("iconclass");
      ok(iconclass.includes("screen-icon"), "panel using screen icon");

      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();

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
        { screen: "Screen" },
        "expected screen to be shared"
      );

      await indicator;
      await checkSharingUI({ screen: "Screen" });

      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(null, false);

      // It sometimes takes a bit longer before the change propagates to the UI,
      // wait for it to avoid intermittents.
      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.screen == "ScreenPaused",
        "screen should be disabled"
      );
      await observerPromise;
      await checkSharingUI({ screen: "ScreenPaused" }, window, {
        screen: "Screen",
      });

      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(null, true);

      await BrowserTestUtils.waitForCondition(
        () => window.gIdentityHandler._sharingState.webRTC.screen == "Screen",
        "screen should be enabled"
      );
      await observerPromise;
      await checkSharingUI({ screen: "Screen" });
      await closeStream();
    },
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.getusermedia.camera.off_while_disabled.delay_ms", 0],
      ["media.getusermedia.microphone.off_while_disabled.delay_ms", 0],
    ],
  });

  SimpleTest.requestCompleteLog();
  await runTests(gTests);
});
