const PAGE_NON_ELIGIBLE_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_eligible_media.html";

// Import this in order to use `triggerPictureInPicture()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

// Bug 1673509 - This test requests a lot of fullscreen for media elements,
// which sometime Gecko would take longer time to fulfill.
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
 * becomes audible later, we would keep controlling it until it's destroyed.
 * (3) If media's duration is too short (<3s), then we would not control it.
 */
add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

add_task(
  async function testNonAudibleMediaCantActivateControllerButAudibleMediaCan() {
    for (const elementId of gNonEligibleElementIds) {
      info(`open new tab with non eligible media elements`);
      const tab = await createLoadedTabWrapper(PAGE_NON_ELIGIBLE_MEDIA, {
        needCheck: couldElementBecomeEligible(elementId),
      });

      info(`although media is playing but it won't activate controller`);
      await Promise.all([
        startNonEligibleMedia(tab, elementId),
        checkIfMediaIsStillPlaying(tab, elementId),
      ]);
      ok(!tab.controller.isActive, "controller is still inactive");

      if (couldElementBecomeEligible(elementId)) {
        info(`make element ${elementId} audible would activate controller`);
        await Promise.all([
          makeElementEligible(tab, elementId),
          checkOrWaitUntilControllerBecomeActive(tab),
        ]);
      }

      info(`remove tab`);
      await tab.close();
    }
  }
);

/**
 * Normally those media are not able to being controlled, however, once they
 * enter fullsceen or Picture-in-Picture mode, then they can be controlled.
 */
add_task(async function testNonEligibleMediaEnterFullscreen() {
  info(`open new tab with non eligible media elements`);
  const tab = await createLoadedTabWrapper(PAGE_NON_ELIGIBLE_MEDIA);

  for (const elementId of gNonEligibleElementIds) {
    await startNonEligibleMedia(tab, elementId);

    info(`entering fullscreen should activate the media controller`);
    await enterFullScreen(tab, elementId);
    await checkOrWaitUntilControllerBecomeActive(tab);
    ok(true, `fullscreen ${elementId} media is able to being controlled`);

    info(`leave fullscreen`);
    await leaveFullScreen(tab);
  }
  info(`remove tab`);
  await tab.close();
});

add_task(async function testNonEligibleMediaEnterPIPMode() {
  info(`open new tab with non eligible media elements`);
  const tab = await createLoadedTabWrapper(PAGE_NON_ELIGIBLE_MEDIA);

  for (const elementId of gNonEligibleElementIds) {
    await startNonEligibleMedia(tab, elementId);

    info(`media entering PIP mode should activate the media controller`);
    const winPIP = await triggerPictureInPicture(tab.linkedBrowser, elementId);
    await checkOrWaitUntilControllerBecomeActive(tab);
    ok(true, `PIP ${elementId} media is able to being controlled`);

    info(`stop PIP mode`);
    await BrowserTestUtils.closeWindow(winPIP);
  }
  info(`remove tab`);
  await tab.close();
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
    info(`start non eligible media ${Id}`);
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

function enterFullScreen(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    return new Promise(r => {
      const element = content.document.getElementById(Id);
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

function leaveFullScreen(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    return content.document.exitFullscreen();
  });
}
