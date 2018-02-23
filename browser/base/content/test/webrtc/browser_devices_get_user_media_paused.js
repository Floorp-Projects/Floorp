/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function setTrackEnabled(audio, video) {
  return ContentTask.spawn(gBrowser.selectedBrowser, {audio, video}, function(args) {
    let stream = content.wrappedJSObject.gStreams[0];
    if (args.audio != null) {
      stream.getAudioTracks()[0].enabled = args.audio;
    }
    if (args.video != null) {
      stream.getVideoTracks()[0].enabled = args.video;
    }
  });
}

var gTests = [
{
  desc: "getUserMedia audio+video: disabling the stream shows the paused indicator",
  run: async function checkPaused() {
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
    await checkSharingUI({
      video: STATE_CAPTURE_ENABLED,
      audio: STATE_CAPTURE_ENABLED,
    });

    // Disable both audio and video.
    await setTrackEnabled(false, false);

    // It sometimes takes a bit longer before the change propagates to the UI,
    // wait for it to avoid intermittents.
    await BrowserTestUtils.waitForCondition(
      () => window.gIdentityHandler._sharingState.camera == STATE_CAPTURE_DISABLED,
      "video should be disabled"
    );

    await expectObserverCalled("recording-device-events", 2);

    // The identity UI should show both as disabled.
    await checkSharingUI({
      video: STATE_CAPTURE_DISABLED,
      audio: STATE_CAPTURE_DISABLED,
    });

    // Enable only audio again.
    await setTrackEnabled(true);

    await BrowserTestUtils.waitForCondition(
      () => window.gIdentityHandler._sharingState.microphone == STATE_CAPTURE_ENABLED,
      "audio should be enabled"
    );

    await expectObserverCalled("recording-device-events");

    // The identity UI should show only video as disabled.
    await checkSharingUI({
      video: STATE_CAPTURE_DISABLED,
      audio: STATE_CAPTURE_ENABLED,
    });

    // Enable video again.
    await setTrackEnabled(null, true);

    await BrowserTestUtils.waitForCondition(
      () => window.gIdentityHandler._sharingState.camera == STATE_CAPTURE_ENABLED,
      "video should be enabled"
    );

    await expectObserverCalled("recording-device-events");

    // Both streams should show as running.
    await checkSharingUI({
      video: STATE_CAPTURE_ENABLED,
      audio: STATE_CAPTURE_ENABLED,
    });
    await closeStream();
  }
},

{
  desc: "getUserMedia screen: disabling the stream shows the paused indicator",
  run: async function checkScreenPaused() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);
    let notification = PopupNotifications.panel.firstChild;
    let iconclass = notification.getAttribute("iconclass");
    ok(iconclass.includes("screen-icon"), "panel using screen icon");

    let menulist =
      document.getElementById("webRTC-selectWindow-menulist");
    menulist.getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");

    await indicator;
    await checkSharingUI({screen: "Screen"});

    await setTrackEnabled(null, false);

    // It sometimes takes a bit longer before the change propagates to the UI,
    // wait for it to avoid intermittents.
    await BrowserTestUtils.waitForCondition(
      () => window.gIdentityHandler._sharingState.screen == "ScreenPaused",
      "screen should be disabled"
    );
    await expectObserverCalled("recording-device-events");
    await checkSharingUI({screen: "ScreenPaused"}, window, {screen: "Screen"});

    await setTrackEnabled(null, true);

    await BrowserTestUtils.waitForCondition(
      () => window.gIdentityHandler._sharingState.screen == "Screen",
      "screen should be enabled"
    );
    await expectObserverCalled("recording-device-events");
    await checkSharingUI({screen: "Screen"});
  }
},
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.getusermedia.camera.off_while_disabled.delay_ms", 0],
    ["media.getusermedia.microphone.off_while_disabled.delay_ms", 0],
  ]});

  SimpleTest.requestCompleteLog();
  await runTests(gTests);
});
