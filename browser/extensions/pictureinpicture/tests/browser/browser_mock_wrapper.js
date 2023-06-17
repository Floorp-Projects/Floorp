/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../../toolkit/components/pictureinpicture/tests/head.js */

ChromeUtils.defineESModuleGetters(this, {
  TOGGLE_POLICIES: "resource://gre/modules/PictureInPictureControls.sys.mjs",
});

const TEST_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://mochitest.youtube.com:443"
  ) + "test-mock-wrapper.html";
const TEST_URL_TOGGLE_VISIBILITY =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://mochitest.youtube.com:443"
  ) + "test-toggle-visibility.html";

/**
 * Tests the mock-wrapper.js video wrapper script selects the expected element
 * responsible for toggling the video player's mute status.
 */
add_task(async function test_mock_mute_button() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await ensureVideosReady(browser);

    // Open the video in PiP
    let videoID = "mock-video-controls";
    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");

    // Mute audio
    await toggleMute(browser, pipWin);
    ok(await isVideoMuted(browser, videoID), "The audio is muted.");

    await SpecialPowers.spawn(browser, [], async function () {
      let muteButton = content.document.querySelector(".mute-button");
      ok(
        muteButton.getAttribute("isMuted"),
        "muteButton has isMuted attribute."
      );
    });

    // Unmute audio
    await toggleMute(browser, pipWin);
    ok(!(await isVideoMuted(browser, videoID)), "The audio is playing.");

    await SpecialPowers.spawn(browser, [], async function () {
      let muteButton = content.document.querySelector(".mute-button");
      ok(
        !muteButton.getAttribute("isMuted"),
        "muteButton does not have isMuted attribute"
      );
    });

    // Close PiP window
    let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
    let closeButton = pipWin.document.getElementById("close");
    EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
    await pipClosed;
  });
});

/**
 * Tests the mock-wrapper.js video wrapper script selects the expected element
 * responsible for toggling the video player's play/pause status.
 */
add_task(async function test_mock_play_pause_button() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await ensureVideosReady(browser);
    await setupVideoListeners(browser);

    // Open the video in PiP
    let videoID = "mock-video-controls";
    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");

    info("Test a wrapper method with a correct selector");
    // Play video
    let playbackPromise = waitForVideoEvent(browser, "playing");
    let playPause = pipWin.document.getElementById("playpause");
    EventUtils.synthesizeMouseAtCenter(playPause, {}, pipWin);
    await playbackPromise;
    ok(!(await isVideoPaused(browser, videoID)), "The video is playing.");

    info("Test a wrapper method with an incorrect selector");
    // Pause the video.
    let pausePromise = waitForVideoEvent(browser, "pause");
    EventUtils.synthesizeMouseAtCenter(playPause, {}, pipWin);
    await pausePromise;
    ok(await isVideoPaused(browser, videoID), "The video is paused.");

    // Close PiP window
    let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
    let closeButton = pipWin.document.getElementById("close");
    EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
    await pipClosed;
  });
});

/**
 * Tests the mock-wrapper.js video wrapper script does not toggle mute/umute
 * state when increasing/decreasing the volume using the arrow keys.
 */
add_task(async function test_volume_change_with_keyboard() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await ensureVideosReady(browser);
    await setupVideoListeners(browser);

    // Open the video in PiP
    let videoID = "mock-video-controls";
    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");

    // Initially set video to be muted
    await toggleMute(browser, pipWin);
    ok(await isVideoMuted(browser, videoID), "The audio is not playing.");

    // Decrease volume with arrow down
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, pipWin);
    ok(await isVideoMuted(browser, videoID), "The audio is not playing.");

    // Increase volume with arrow up
    EventUtils.synthesizeKey("KEY_ArrowUp", {}, pipWin);
    ok(!(await isVideoMuted(browser, videoID)), "The audio is still playing.");

    await SpecialPowers.spawn(browser, [], async function () {
      let video = content.document.querySelector("video");
      ok(!video.muted, "Video should be unmuted.");
    });

    // Close PiP window
    let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
    let closeButton = pipWin.document.getElementById("close");
    EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
    await pipClosed;
  });
});

function waitForVideoEvent(browser, eventType) {
  return BrowserTestUtils.waitForContentEvent(browser, eventType, true);
}

async function toggleMute(browser, pipWin) {
  let mutedPromise = waitForVideoEvent(browser, "volumechange");
  let audioButton = pipWin.document.getElementById("audio");
  EventUtils.synthesizeMouseAtCenter(audioButton, {}, pipWin);
  await mutedPromise;
}

async function setupVideoListeners(browser) {
  await SpecialPowers.spawn(browser, [], async function () {
    let video = content.document.querySelector("video");

    // Set a listener for "playing" event
    video.addEventListener("playing", async () => {
      info("Got playing event!");
      let playPauseButton =
        content.document.querySelector(".play-pause-button");
      ok(
        !playPauseButton.getAttribute("isPaused"),
        "playPauseButton does not have isPaused attribute."
      );
    });

    // Set a listener for "pause" event
    video.addEventListener("pause", async () => {
      info("Got pause event!");
      let playPauseButton =
        content.document.querySelector(".play-pause-button");
      // mock-wrapper's pause() method uses an invalid selector and should throw
      // an error. Test that the PiP wrapper uses the fallback pause() method.
      // This is to ensure PiP can handle cases where a site wrapper script is
      // incorrect, but does not break functionality altogether.
      ok(
        !playPauseButton.getAttribute("isPaused"),
        "playPauseButton still doesn't have isPaused attribute."
      );
    });
  });
}

/**
 * Tests that the mock-wrapper.js video wrapper hides the pip toggle when shouldHideToggle()
 * returns true.
 */
add_task(async function test_mock_should_hide_toggle() {
  await testToggle(TEST_URL_TOGGLE_VISIBILITY, {
    "mock-video-controls": { canToggle: false, policy: TOGGLE_POLICIES.HIDDEN },
  });
});

/**
 * Tests that the mock-wrapper.js video wrapper does not hide the pip toggle when shouldHideToggle()
 * returns false.
 */
add_task(async function test_mock_should_not_hide_toggle() {
  await testToggle(TEST_URL, {
    "mock-video-controls": { canToggle: true },
  });
});
