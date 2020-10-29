/* eslint-disable no-undef */
const PAGE_NON_ELIGIBLE_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_eligible_media.html";

// Import this in order to use `triggerPictureInPicture()`.
/* import-globals-from ../../../../toolkit/components/pictureinpicture/tests/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

// Bug 1620686 - This test requests a lot of fullscreen and picture-in-picture
// for media elements which needs longer time to run.
requestLongerTimeout(2);

// This array contains the elements' id in `file_non_eligible_media.html`.
const gNonEligibleElementIds = [
  "muted",
  "volume-0",
  "silent-audio-track",
  "no-audio-track",
  "short-duration",
  "inaudible-captured-media",
];

/**
 * This test is used to test couples of things about what kinds of media is
 * eligible for being controlled by media control keys.
 * (1) If media is inaudible all the time, then we would not control it.
 * (2) If media starts inaudibly, we would not try to control it. But once it
 * becomes audible later, we would keep controlling it until it's detroyed.
 * (3) If media's duration is too short (<3s), then we would not control it.
 */
add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

add_task(async function testPlayPauseAndStop() {
  for (const elementId of gNonEligibleElementIds) {
    info(`- open new tab and start non eligible media ${elementId} -`);
    const tab = await createTabAndLoad(PAGE_NON_ELIGIBLE_MEDIA);
    await startNonEligibleMedia(tab, elementId);

    // Generate media control event should be postponed for a while to ensure
    // that we didn't create any controller.
    info(`- let media play for a while -`);
    await checkIfMediaIsStillPlaying(tab, elementId);

    info(`- simulate pressing 'pause' media control key -`);
    MediaControlService.generateMediaControlKey("pause");

    info(`- non eligible media won't be controlled by media control -`);
    await checkIfMediaIsStillPlaying(tab, elementId);

    if (couldElementBecomeEligible(elementId)) {
      info(`- make element ${elementId} audible -`);
      await makeElementEligible(tab, elementId);

      info(`- simulate pressing 'pause' media control key -`);
      MediaControlService.generateMediaControlKey("pause");

      info(`- audible media should be controlled by media control -`);
      await waitUntilMediaPaused(tab, elementId);
    }

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
  }
});

/**
 * Normally those media are not able to being controlled, however, once they
 * enter fullsceen or Picture-in-Picture mode, then they can be controlled.
 */
add_task(async function testNonEligibleMediaEnterFullscreen() {
  for (const elementId of gNonEligibleElementIds) {
    info(`- open new tab and start non eligible media ${elementId} -`);
    const tab = await createTabAndLoad(PAGE_NON_ELIGIBLE_MEDIA);
    await startNonEligibleMedia(tab, elementId);

    info(`entering fullscreen should activate the media controller`);
    await enableFullScreen(tab, elementId);
    await checkOrWaitUntilControllerBecomeActive(tab);
    ok(true, `fullscreen ${elementId} media is able to being controlled`);

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function testNonEligibleMediaEnterPIPMode() {
  for (const elementId of gNonEligibleElementIds) {
    info(`- open new tab and start non eligible media ${elementId} -`);
    const tab = await createTabAndLoad(PAGE_NON_ELIGIBLE_MEDIA);
    await startNonEligibleMedia(tab, elementId);

    info(`media entering PIP mode should activate the media controller`);
    const winPIP = await triggerPictureInPicture(tab.linkedBrowser, elementId);
    await checkOrWaitUntilControllerBecomeActive(tab);
    ok(true, `PIP ${elementId} media is able to being controlled`);

    info(`remove tab`);
    await BrowserTestUtils.closeWindow(winPIP);
    await BrowserTestUtils.removeTab(tab);
  }
});

/**
 * The following are helper functions.
 */
function startNonEligibleMedia(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    if (Id == "volume-0") {
      video.volume = 0.0;
    }
    if (Id == "inaudible-captured-media") {
      const context = new content.AudioContext();
      context.createMediaElementSource(video);
    }
    return video.play();
  });
}

function checkIfMediaIsStillPlaying(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    return new Promise(r => {
      // In order to test "media isn't affected by media control", we would not
      // only check `mPaused`, we would also oberve "timeupdate" event multiple
      // times to ensure that video is still playing continually.
      let timeUpdateCount = 0;
      ok(!video.paused);
      video.ontimeupdate = () => {
        if (++timeUpdateCount == 3) {
          video.ontimeupdate = null;
          r();
        }
      };
    });
  });
}

function couldElementBecomeEligible(elementId) {
  return elementId == "muted" || elementId == "volume-0";
}

function makeElementEligible(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    // to turn inaudible media become audible in order to be controlled.
    video.volume = 1.0;
    video.muted = false;
  });
}

function waitUntilMediaPaused(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    if (video.paused) {
      ok(true, "media has been paused");
      return Promise.resolve();
    }
    return new Promise(r => (video.onpaused = r));
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
