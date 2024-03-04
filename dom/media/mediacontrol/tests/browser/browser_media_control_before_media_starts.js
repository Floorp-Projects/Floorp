// Import this in order to use `triggerPictureInPicture()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
const IFRAME_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";
const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.mediacontrol.testingevents.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
    ],
  });
});

/**
 * Usually we would only start controlling media after media starts, but if
 * media has entered Picture-in-Picture mode or fullscreen, then we would allow
 * users to control them directly without prior to starting media.
 */
add_task(async function testMediaEntersPIPMode() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`trigger PIP mode`);
  const winPIP = await triggerPictureInPicture(tab.linkedBrowser, testVideoId);

  info(`press 'play' and wait until media starts`);
  await generateMediaControlKeyEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.closeWindow(winPIP);
  await tab.close();
});

add_task(async function testMutedMediaEntersPIPMode() {
  info(`open media page and mute video`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);
  await muteMedia(tab, testVideoId);

  info(`trigger PIP mode`);
  const winPIP = await triggerPictureInPicture(tab.linkedBrowser, testVideoId);

  info(`press 'play' and wait until media starts`);
  await generateMediaControlKeyEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await BrowserTestUtils.closeWindow(winPIP);
  await tab.close();
});

add_task(async function testMediaEntersFullScreen() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`make video fullscreen`);
  await enableFullScreen(tab, testVideoId);

  info(`press 'play' and wait until media starts`);
  await generateMediaControlKeyEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testMutedMediaEntersFullScreen() {
  info(`open media page and mute video`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);
  await muteMedia(tab, testVideoId);

  info(`make video fullscreen`);
  await enableFullScreen(tab, testVideoId);

  info(`press 'play' and wait until media starts`);
  await generateMediaControlKeyEvent("play");
  await checkOrWaitUntilMediaStartedPlaying(tab, testVideoId);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testNonMediaEntersFullScreen() {
  info(`open media page which won't have an activated controller`);
  // As we won't activate controller in this test case, not need to
  // check controller's status.
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY, {
    needCheck: false,
  });

  info(`make non-media element fullscreen`);
  const nonMediaElementId = "image";
  await enableFullScreen(tab, nonMediaElementId);

  info(`press 'play' which should not start media`);
  // Use `generateMediaControlKey()` directly because `play` won't affect the
  // controller's playback state (don't need to wait for the change).
  MediaControlService.generateMediaControlKey("play");
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testMediaInIframeEntersFullScreen() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`make video in iframe fullscreen`);
  await enableMediaFullScreenInIframe(tab, testVideoId);

  info(`press 'play' and wait until media starts`);
  await generateMediaControlKeyEvent("play");

  info(`full screen media in inframe should be started`);
  await waitUntilIframeMediaStartedPlaying(tab);

  info(`media not in fullscreen should keep paused`);
  await checkOrWaitUntilMediaStoppedPlaying(tab, testVideoId);

  info(`remove iframe that contains fullscreen video`);
  await removeIframeFromDocument(tab);

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
function muteMedia(tab, videoId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [videoId], videoId => {
    content.document.getElementById(videoId).muted = true;
  });
}

function enableFullScreen(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], elementId => {
    return new Promise(r => {
      const element = content.document.getElementById(elementId);
      element.requestFullscreen();
      element.onfullscreenchange = () => {
        element.onfullscreenchange = null;
        element.onfullscreenerror = null;
        r();
      };
      element.onfullscreenerror = () => {
        // Retry until the element successfully enters fullscreen.
        element.requestFullscreen();
      };
    });
  });
}

function enableMediaFullScreenInIframe(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [IFRAME_URL], async url => {
    info(`create iframe and wait until it finishes loading`);
    const iframe = content.document.getElementById("iframe");
    iframe.src = url;
    await new Promise(r => (iframe.onload = r));

    info(`trigger media in iframe entering into fullscreen`);
    iframe.contentWindow.postMessage("fullscreen", "*");
    info(`wait until media in frame enters fullscreen`);
    return new Promise(r => {
      content.onmessage = event => {
        is(
          event.data,
          "entered-fullscreen",
          `media in iframe entered fullscreen`
        );
        r();
      };
    });
  });
}

function waitUntilIframeMediaStartedPlaying(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [IFRAME_URL], async () => {
    info(`check if media in iframe starts playing`);
    const iframe = content.document.getElementById("iframe");
    iframe.contentWindow.postMessage("check-playing", "*");
    return new Promise(r => {
      content.onmessage = event => {
        is(event.data, "checked-playing", `media in iframe is playing`);
        r();
      };
    });
  });
}

function removeIframeFromDocument(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    info(`remove iframe from document`);
    content.document.getElementById("iframe").remove();
  });
}
