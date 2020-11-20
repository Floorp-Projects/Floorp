// Import this in order to use `triggerPictureInPicture()`.
/* import-globals-from ../../../../../toolkit/components/pictureinpicture/tests/head.js */
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

    info(`enter and leave PIP mode which would not affect controller`);
    const winPIP = await triggerPictureInPicture(
      tab.linkedBrowser,
      testVideoId
    );
    await ensureMessageAndClosePiP(
      tab.linkedBrowser,
      testVideoId,
      winPIP,
      false
    );
    ok(!controller.isActive, "controller is still inactive");

    info(`remove tab`);
    await tab.close();
    // To ensure the focus would be gave back to the original window. If we do
    // not do that, then lacking of focus would interfere other following tests.
    await SimpleTest.promiseFocus(window);
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
    await ensureTabIsAlreadyFocused(tab);
    await enterAndLeaveFullScreen(tab, testVideoId);
    ok(!controller.isActive, "controller is still inactive");

    info(`remove tab`);
    await tab.close();
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
