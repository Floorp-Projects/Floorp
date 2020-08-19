/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function setCameraMuted(mute) {
  const windowId = gBrowser.selectedBrowser.innerWindowID;
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ mute, windowId }],
    function(args) {
      Services.obs.notifyObservers(
        content.window,
        args.mute ? "getUserMedia:muteVideo" : "getUserMedia:unmuteVideo",
        JSON.stringify(args.windowId)
      );
    }
  );
}

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

async function getVideoTrackMuted() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.wrappedJSObject.gStreams[0].getVideoTracks()[0].muted
  );
}

async function getVideoTrackEvents() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.wrappedJSObject.gVideoEvents
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
    run: async function checkDisabled() {
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

      // Wait for capture state to propagate to the UI asynchronously.
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
    run: async function checkDisabledAfterCloneStop() {
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

      // Wait for capture state to propagate to the UI asynchronously.
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
    run: async function checkScreenDisabled() {
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

      // Wait for capture state to propagate to the UI asynchronously.
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

  {
    desc:
      "getUserMedia audio+video: muting the camera shows the muted indicator",
    run: async function checkMuted() {
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
      is(await getVideoTrackMuted(), false, "video track starts unmuted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        [],
        "no video track events fired yet"
      );

      // Mute camera.
      observerPromise = expectObserverCalled("recording-device-events");
      await setCameraMuted(true);

      // Wait for capture state to propagate to the UI asynchronously.
      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
          STATE_CAPTURE_DISABLED,
        "video should be muted"
      );

      await observerPromise;

      // The identity UI should show only camera as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      is(await getVideoTrackMuted(), true, "video track is muted");
      Assert.deepEqual(await getVideoTrackEvents(), ["mute"], "mute fired");

      // Unmute video again.
      observerPromise = expectObserverCalled("recording-device-events");
      await setCameraMuted(false);

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
      is(await getVideoTrackMuted(), false, "video track is unmuted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        ["mute", "unmute"],
        "unmute fired"
      );
      await closeStream();
    },
  },

  {
    desc: "getUserMedia audio+video: disabling & muting camera in combination",
    // Test the following combinations of disabling and muting camera:
    // 1. Disable video track only.
    // 2. Mute camera & disable audio (to have a condition to wait for)
    // 3. Enable both audio and video tracks (only audio should flow).
    // 4. Unmute camera again (video should flow).
    // 5. Mute camera & disable both tracks.
    // 6. Unmute camera & enable audio (only audio should flow)
    // 7. Enable video track again (video should flow).
    run: async function checkDisabledMutedCombination() {
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

      // 1. Disable video track only.
      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(null, false);

      // Wait for capture state to propagate to the UI asynchronously.
      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
          STATE_CAPTURE_DISABLED,
        "video should be disabled"
      );

      await observerPromise;

      // The identity UI should show only video as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      is(await getVideoTrackMuted(), false, "video track still unmuted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        [],
        "no video track events fired yet"
      );

      // 2. Mute camera & disable audio (to have a condition to wait for)
      observerPromise = expectObserverCalled("recording-device-events", 2);
      await setCameraMuted(true);
      await setTrackEnabled(false, null);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.microphone ==
          STATE_CAPTURE_DISABLED,
        "audio should be disabled"
      );

      await observerPromise;

      // The identity UI should show both as disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_DISABLED,
      });
      is(await getVideoTrackMuted(), true, "video track is muted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        ["mute"],
        "mute is still fired even though track was disabled"
      );

      // 3. Enable both audio and video tracks (only audio should flow).
      observerPromise = expectObserverCalled("recording-device-events", 2);
      await setTrackEnabled(true, true);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.microphone ==
          STATE_CAPTURE_ENABLED,
        "audio should be enabled"
      );

      await observerPromise;

      // The identity UI should show only audio as enabled, as video is muted.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      is(await getVideoTrackMuted(), true, "video track is still muted");
      Assert.deepEqual(await getVideoTrackEvents(), ["mute"], "no new events");

      // 4. Unmute camera again (video should flow).
      observerPromise = expectObserverCalled("recording-device-events");
      await setCameraMuted(false);

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
      is(await getVideoTrackMuted(), false, "video track is unmuted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        ["mute", "unmute"],
        "unmute fired"
      );

      // 5. Mute camera & disable both tracks.
      observerPromise = expectObserverCalled("recording-device-events", 3);
      await setCameraMuted(true);
      await setTrackEnabled(false, false);

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
      is(await getVideoTrackMuted(), true, "video track is muted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        ["mute", "unmute", "mute"],
        "mute fired afain"
      );

      // 6. Unmute camera & enable audio (only audio should flow)
      observerPromise = expectObserverCalled("recording-device-events", 2);
      await setCameraMuted(false);
      await setTrackEnabled(true, null);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.microphone ==
          STATE_CAPTURE_ENABLED,
        "audio should be enabled"
      );

      await observerPromise;

      // Only audio should show as running, as video track is still disabled.
      await checkSharingUI({
        video: STATE_CAPTURE_DISABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      is(await getVideoTrackMuted(), false, "video track is unmuted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        ["mute", "unmute", "mute", "unmute"],
        "unmute fired even though track is disabled"
      );

      // 7. Enable video track again (video should flow).
      observerPromise = expectObserverCalled("recording-device-events");
      await setTrackEnabled(null, true);

      await BrowserTestUtils.waitForCondition(
        () =>
          window.gIdentityHandler._sharingState.webRTC.camera ==
          STATE_CAPTURE_ENABLED,
        "video should be enabled"
      );

      await observerPromise;

      // The identity UI should show both as running again.
      await checkSharingUI({
        video: STATE_CAPTURE_ENABLED,
        audio: STATE_CAPTURE_ENABLED,
      });
      is(await getVideoTrackMuted(), false, "video track remains unmuted");
      Assert.deepEqual(
        await getVideoTrackEvents(),
        ["mute", "unmute", "mute", "unmute"],
        "no new events fired"
      );
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
