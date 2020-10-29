const PAGE =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
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
  await generateMediaControlKey("pause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`pressing 'play' key`);
  await generateMediaControlKey("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`pressing 'stop' key`);
  await generateMediaControlKey("stop");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`we have stop controlling media, pressing 'play' won't resume media`);
  // Not expect playback state change, so using ChromeUtils's method directly.
  MediaControlService.generateMediaControlKey("play");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testPlayPause() {
  info(`open page and start media`);
  const tab = await createTabAndLoad(PAGE);
  await playMedia(tab, testVideoId);

  info(`pressing 'playPause' key, media should stop`);
  await generateMediaControlKey("playpause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`pressing 'playPause' key, media should start`);
  await generateMediaControlKey("playpause");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

/**
 * The following are helper functions.
 */
function generateMediaControlKey(event) {
  const playbackStateChanged = waitUntilDisplayedPlaybackChanged();
  MediaControlService.generateMediaControlKey(event);
  return playbackStateChanged;
}
