const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_multiple_audible_media.html";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * This test is used to check when resuming media, we would only resume latest
 * paused media, not all media in the page.
 */
add_task(async function testResumingLatestPausedMedias() {
  info(`open media page and play all media`);
  const tab = await createLoadedTabWrapper(PAGE_URL);
  await playAllMedia(tab);

  /**
   * Pressing `pause` key would pause video1, video2, video3
   * So resuming from media control key would affect those three media
   */
  info(`pressing 'pause' should pause all media`);
  await generateMediaControlKeyEvent("pause");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: true,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  info(`all media are latest paused, pressing 'play' should resume all`);
  await generateMediaControlKeyEvent("play");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: false,
    shouldVideo2BePaused: false,
    shouldVideo3BePaused: false,
  });

  info(`pause only one playing video by calling its webidl method`);
  await pauseMedia(tab, "video3");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: false,
    shouldVideo2BePaused: false,
    shouldVideo3BePaused: true,
  });

  /**
   * Pressing `pause` key would pause video1, video2
   * So resuming from media control key would affect those two media
   */
  info(`pressing 'pause' should pause two playing media`);
  await generateMediaControlKeyEvent("pause");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: true,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  info(`two media are latest paused, pressing 'play' should only affect them`);
  await generateMediaControlKeyEvent("play");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: false,
    shouldVideo2BePaused: false,
    shouldVideo3BePaused: true,
  });

  info(`pause only one playing video by calling its webidl method`);
  await pauseMedia(tab, "video2");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: false,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  /**
   * Pressing `pause` key would pause video1
   * So resuming from media control key would only affect one media
   */
  info(`pressing 'pause' should pause one playing media`);
  await generateMediaControlKeyEvent("pause");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: true,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  info(`one media is latest paused, pressing 'play' should only affect it`);
  await generateMediaControlKeyEvent("play");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: false,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  /**
   * Only one media is playing, so pausing it should not stop controlling media.
   * We should still be able to resume it later.
   */
  info(`pause only playing video by calling its webidl method`);
  await pauseMedia(tab, "video1");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: true,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  info(`pressing 'pause' for already paused media, nothing would happen`);
  // All media are already paused, so no need to wait for playback state change,
  // call the method directly.
  MediaControlService.generateMediaControlKey("pause");

  info(`pressing 'play' would still affect on latest paused media`);
  await generateMediaControlKeyEvent("play");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: false,
    shouldVideo2BePaused: true,
    shouldVideo3BePaused: true,
  });

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
async function playAllMedia(tab) {
  const playbackStateChangedPromise = waitUntilDisplayedPlaybackChanged();
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return new Promise(r => {
      const videos = content.document.getElementsByTagName("video");
      let mediaCount = 0;
      docShell.chromeEventHandler.addEventListener(
        "MozStartMediaControl",
        () => {
          if (++mediaCount == videos.length) {
            info(`all media have started media control`);
            r();
          }
        }
      );
      for (let video of videos) {
        info(`play ${video.id} video`);
        video.play();
      }
    });
  });
  await playbackStateChangedPromise;
}

async function pauseMedia(tab, videoId) {
  await SpecialPowers.spawn(tab.linkedBrowser, [videoId], videoId => {
    const video = content.document.getElementById(videoId);
    if (!video) {
      ok(false, `can not find ${videoId}!`);
    }
    video.pause();
  });
}

function checkMediaPausedState(
  tab,
  { shouldVideo1BePaused, shouldVideo2BePaused, shouldVideo3BePaused }
) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [shouldVideo1BePaused, shouldVideo2BePaused, shouldVideo3BePaused],
    (shouldVideo1BePaused, shouldVideo2BePaused, shouldVideo3BePaused) => {
      const video1 = content.document.getElementById("video1");
      const video2 = content.document.getElementById("video2");
      const video3 = content.document.getElementById("video3");
      is(
        video1.paused,
        shouldVideo1BePaused,
        "Correct paused state for video1"
      );
      is(
        video2.paused,
        shouldVideo2BePaused,
        "Correct paused state for video2"
      );
      is(
        video3.paused,
        shouldVideo3BePaused,
        "Correct paused state for video3"
      );
    }
  );
}
