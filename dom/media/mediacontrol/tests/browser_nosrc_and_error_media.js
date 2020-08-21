/* eslint-disable no-undef */

// Import this in order to use `triggerPictureInPicture()`.
/* import-globals-from ../../../../toolkit/components/pictureinpicture/tests/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const PAGE_NOSRC_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_no_src_media.html";
const PAGE_ERROR_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_error_media.html";
const PAGES = [PAGE_NOSRC_MEDIA, PAGE_ERROR_MEDIA];
const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * To ensure the no src media and media with error won't activate the media
 * controller even if they enters PIP mode or fullscreen.
 */
add_task(async function testNoSrcOrErrorMediaEntersPIPMode() {
  for (let page of PAGES) {
    info(`open media page ${page}`);
    const tab = await createTabAndLoad(page);

    info(`controller should always inactive`);
    const controller = tab.linkedBrowser.browsingContext.mediaController;
    controller.onactivated = () => {
      ok(false, "should not get activated!");
    };

    info(`enter PIP mode several times and controller should keep inactive`);
    for (let idx = 0; idx < 3; idx++) {
      const winPIP = await triggerPictureInPicture(
        tab.linkedBrowser,
        testVideoId
      );
      await BrowserTestUtils.closeWindow(winPIP);
    }
    ok(!controller.isActive, "controller is still inactive");

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function testNoSrcOrErrorMediaEntersFullscreen() {
  for (let page of PAGES) {
    info(`open media page ${page}`);
    const tab = await createTabAndLoad(page);

    info(`controller should always inactive`);
    const controller = tab.linkedBrowser.browsingContext.mediaController;
    controller.onactivated = () => {
      ok(false, "should not get activated!");
    };

    info(`enter fullscreen several times and controller should keep inactive`);
    await ensureTabIsAlreadyFocused(tab);
    for (let idx = 0; idx < 3; idx++) {
      await enterAndLeaveFullScreen(tab, testVideoId);
    }
    ok(!controller.isActive, "controller is still inactive");

    info(`remove tab`);
    await BrowserTestUtils.removeTab(tab);
  }
});

/**
 * The following is helper function.
 */
function ensureTabIsAlreadyFocused(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    // fullscreen can only be request from a visible tab, sometime we excecute
    // this method too early where the tab hasn't become a focus tab.
    if (content.document.visibilityState != "visible") {
      info(`wait until tab becomes a focus tab`);
      return new Promise(r => {
        content.document.onvisibilitychange = () => {
          if (content.document.visibilityState == "visible") {
            r();
          }
        };
      });
    }
    return Promise.resolve();
  });
}

function enterAndLeaveFullScreen(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], elementId => {
    return new Promise(r => {
      const element = content.document.getElementById(elementId);
      ok(!content.document.fullscreenElement, "no fullscreen element");
      element.requestFullscreen();
      element.onfullscreenchange = () => {
        if (content.document.fullscreenElement) {
          element.onfullscreenerror = null;
          content.document.exitFullscreen();
        } else {
          element.onfullscreenchange = null;
          element.onfullscreenerror = null;
          r();
        }
      };
      element.onfullscreenerror = () => {
        // Retry until the element successfully enters fullscreen.
        element.requestFullscreen();
      };
    });
  });
}
