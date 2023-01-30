// Import this in order to use `triggerPictureInPicture()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const PAGE_NOSRC_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_no_src_media.html";
const PAGE_ERROR_MEDIA =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_error_media.html";
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
    const tab = await createLoadedTabWrapper(page, { needCheck: false });

    info(`controller should always inactive`);
    const controller = tab.linkedBrowser.browsingContext.mediaController;
    controller.onactivated = () => {
      ok(false, "should not get activated!");
    };

    info(`enter PIP mode which would not affect controller`);
    const winPIP = await triggerPictureInPicture(
      tab.linkedBrowser,
      testVideoId
    );
    info(`leave PIP mode`);
    await ensureMessageAndClosePiP(
      tab.linkedBrowser,
      testVideoId,
      winPIP,
      false
    );
    ok(!controller.isActive, "controller is still inactive");

    info(`remove tab`);
    await tab.close();
  }
});

add_task(async function testNoSrcOrErrorMediaEntersFullscreen() {
  for (let page of PAGES) {
    info(`open media page ${page}`);
    const tab = await createLoadedTabWrapper(page, { needCheck: false });

    info(`controller should always inactive`);
    const controller = tab.linkedBrowser.browsingContext.mediaController;
    controller.onactivated = () => {
      ok(false, "should not get activated!");
    };

    info(`enter and leave fullscreen which would not affect controller`);
    await enterAndLeaveFullScreen(tab, testVideoId);
    ok(!controller.isActive, "controller is still inactive");

    info(`remove tab`);
    await tab.close();
  }
});

/**
 * The following is helper function.
 */
async function enterAndLeaveFullScreen(tab, elementId) {
  await new Promise(resolve =>
    SimpleTest.waitForFocus(resolve, tab.linkedBrowser.ownerGlobal)
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [elementId], elementId => {
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
