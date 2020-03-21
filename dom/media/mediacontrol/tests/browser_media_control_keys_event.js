/* eslint-disable no-undef */
const PAGE =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_autoplay.html";
const testVideoId = "video";

/**
 * This test is used to generate platform-independent media control keys event
 * and see if media can be controlled correctly and current we only support
 * `play`, `pause`, `playPause` and `stop` events.
 */
add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

add_task(async function testPlayPauseAndStop() {
  info(`open page and start media`);
  const tab = await createTabAndLoad(PAGE);
  await playMedia(tab, testVideoId);

  info(`pressing 'pause' key`);
  await generateMediaControlKeyEvent("pause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`pressing 'play' key`);
  await generateMediaControlKeyEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`pressing 'stop' key`);
  await generateMediaControlKeyEvent("stop");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testPlayPause() {
  info(`open page and start media`);
  const tab = await createTabAndLoad(PAGE);
  await playMedia(tab, testVideoId);

  info(`pressing 'playPause' key, media should stop`);
  await generateMediaControlKeyEvent("playPause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`pressing 'playPause' key, media should start`);
  await generateMediaControlKeyEvent("playPause");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

/**
 * The following are helper functions.
 */
function generateMediaControlKeyEvent(event) {
  const playbackStateChanged = waitUntilMainMediaControllerPlaybackChanged();
  ChromeUtils.generateMediaControlKeysTestEvent(event);
  return playbackStateChanged;
}
