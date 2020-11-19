const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_audio_and_inaudible_media.html";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * When a page has audible media and inaudible media playing at the same time,
 * only audible media should be controlled by media control keys. However, once
 * inaudible media becomes audible, then it should be able to be controlled.
 */
add_task(async function testSetPositionState() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_URL);

  info(`play video1 (audible) and video2 (inaudible)`);
  await playBothAudibleAndInaudibleMedia(tab);

  info(`pressing 'pause' should only affect video1 (audible)`);
  await generateMediaControlKeyEvent("pause");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: true,
    shouldVideo2BePaused: false,
  });

  info(`make video2 become audible, then it would be able to be controlled`);
  await unmuteInaudibleMedia(tab);

  info(`pressing 'pause' should affect video2 (audible`);
  await generateMediaControlKeyEvent("pause");
  await checkMediaPausedState(tab, {
    shouldVideo1BePaused: true,
    shouldVideo2BePaused: true,
  });

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
async function playBothAudibleAndInaudibleMedia(tab) {
  const playbackStateChangedPromise = waitUntilDisplayedPlaybackChanged();
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const videos = content.document.getElementsByTagName("video");
    let promises = [];
    for (let video of videos) {
      info(`play ${video.id} video`);
      promises.push(video.play());
    }
    return Promise.all(promises);
  });
  await playbackStateChangedPromise;
}

function checkMediaPausedState(
  tab,
  { shouldVideo1BePaused, shouldVideo2BePaused }
) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [shouldVideo1BePaused, shouldVideo2BePaused],
    (shouldVideo1BePaused, shouldVideo2BePaused) => {
      const video1 = content.document.getElementById("video1");
      const video2 = content.document.getElementById("video2");
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
    }
  );
}

function unmuteInaudibleMedia(tab) {
  const unmutePromise = SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const video2 = content.document.getElementById("video2");
    video2.muted = false;
  });
  // Inaudible media was not controllable, so it won't affect the controller's
  // playback state. However, when it becomes audible, which means being able to
  // be controlled by media controller, it would make the playback state chanege
  // to `playing` because now we have an audible playinng media in the page.
  return Promise.all([unmutePromise, waitUntilDisplayedPlaybackChanged()]);
}
