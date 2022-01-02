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
  const tab = await createLoadedTabWrapper(PAGE);
  await playMedia(tab, testVideoId);

  info(`pressing 'pause' key`);
  MediaControlService.generateMediaControlKey("pause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`pressing 'play' key`);
  MediaControlService.generateMediaControlKey("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`pressing 'stop' key`);
  MediaControlService.generateMediaControlKey("stop");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`Has stopped controlling media, pressing 'play' won't resume it`);
  MediaControlService.generateMediaControlKey("play");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testPlayPause() {
  info(`open page and start media`);
  const tab = await createLoadedTabWrapper(PAGE);
  await playMedia(tab, testVideoId);

  info(`pressing 'playPause' key, media should stop`);
  MediaControlService.generateMediaControlKey("playpause");
  await Promise.all([
    new Promise(r => (tab.controller.onplaybackstatechange = r)),
    checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId),
  ]);

  info(`pressing 'playPause' key, media should start`);
  MediaControlService.generateMediaControlKey("playpause");
  await Promise.all([
    new Promise(r => (tab.controller.onplaybackstatechange = r)),
    checkOrWaitUntilMediaStartedPlaying(tab, testVideoId),
  ]);

  info(`remove tab`);
  await tab.close();
});
