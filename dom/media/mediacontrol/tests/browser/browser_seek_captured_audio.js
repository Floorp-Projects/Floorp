const PAGE_NON_AUTOPLAY_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * Seeking a captured audio media before it starts, and it should still be able
 * to be controlled via media key after it starts playing.
 */
add_task(async function testSeekAudibleCapturedMedia() {
  info(`open new non autoplay media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY_MEDIA);

  info(`perform seek on the captured media before it starts`);
  await captureAudio(tab, testVideoId);
  await seekAudio(tab, testVideoId);

  info(`start captured media`);
  await playMedia(tab, testVideoId);

  info(`pressing 'pause' key, captured media should be paused`);
  await generateMediaControlKeyEvent("pause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
function captureAudio(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    const context = new content.AudioContext();
    // Capture audio from the media element to a MediaElementAudioSourceNode.
    context.createMediaElementSource(video);
  });
}

function seekAudio(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], async Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    video.currentTime = 0.0;
    await new Promise(r => (video.onseeked = r));
  });
}
