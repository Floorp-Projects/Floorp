/* eslint-disable no-undef */
const PAGE_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_autoplay.html";

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
  info(`open autoplay media`);
  const tab = await createTabAndLoad(PAGE_AUTOPLAY);
  await checkOrWaitUntilMediaStartedPlaying(tab);

  info(`pressing 'pause' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("pause");
  await checkOrWaitUntilMediaStoppedPlaying(tab);

  info(`pressing 'play' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab);

  info(`pressing 'stop' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("stop");
  await checkOrWaitUntilMediaStoppedPlaying(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testPlayPause() {
  info(`open autoplay media`);
  const tab = await createTabAndLoad(PAGE_AUTOPLAY);
  await checkOrWaitUntilMediaStartedPlaying(tab);

  info(`pressing 'playPause' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("playPause");
  await checkOrWaitUntilMediaStoppedPlaying(tab);

  info(`pressing 'playPause' key`);
  ChromeUtils.generateMediaControlKeysTestEvent("playPause");
  await checkOrWaitUntilMediaStartedPlaying(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});
