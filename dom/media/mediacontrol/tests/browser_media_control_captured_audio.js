/* eslint-disable no-undef */
const PAGE_NON_AUTOPLAY_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_autoplay.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * When we capture audio from an media element to the web audio, if the media
 * is audible, it should be controlled by media keys as well.
 */
add_task(async function testAudibleCapturedMedia() {
  info(`open new non autoplay media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY_MEDIA);

  info(`capture audio and start playing`);
  await captureAudio(tab, testVideoId);
  await playMedia(tab, testVideoId);

  info(`pressing 'pause' key, captured media should be paused`);
  await generateMediaControlKeyEvent("pause");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
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
