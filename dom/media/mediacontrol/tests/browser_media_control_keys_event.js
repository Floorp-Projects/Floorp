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
  await playMedia(tab);

  info(`pressing 'pause' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("pause");
  await waitUntilPlaybackStops(tab);

  info(`pressing 'play' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("play");
  await waitUntilPlaybackStarts(tab);

  info(`pressing 'stop' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("stop");
  await waitUntilPlaybackStops(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testPlayPause() {
  info(`open page and start media`);
  const tab = await createTabAndLoad(PAGE);
  await playMedia(tab);

  info(`pressing 'playPause' key, media should stop`);
  ChromeUtils.generateMediaControlKeysTestEvent("playPause");
  await waitUntilPlaybackStops(tab);

  info(`pressing 'playPause' key, media should start`);
  ChromeUtils.generateMediaControlKeysTestEvent("playPause");
  await waitUntilPlaybackStarts(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

/**
 * The following are helper functions.
 */
function waitUntilPlaybackStarts(tab) {
  return Promise.all([
    checkOrWaitUntilMediaStartedPlaying(tab, testVideoId),
    waitUntilMainMediaControllerPlaybackChanged(),
  ]);
}

function waitUntilPlaybackStops(tab) {
  return Promise.all([
    checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId),
    waitUntilMainMediaControllerPlaybackChanged(),
  ]);
}

function playMedia(tab) {
  const playPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [testVideoId],
    Id => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      return video.play();
    }
  );
  return Promise.all([
    playPromise,
    waitUntilMainMediaControllerPlaybackChanged(),
  ]);
}
