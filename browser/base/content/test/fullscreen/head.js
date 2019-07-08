const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { ContentTask } = ChromeUtils.import(
  "resource://testing-common/ContentTask.jsm"
);

function waitForFullScreenState(browser, state) {
  let eventName = state
    ? "MozDOMFullscreen:Entered"
    : "MozDOMFullscreen:Exited";
  return BrowserTestUtils.waitForEvent(browser.ownerGlobal, eventName);
}

/**
 * Spawns content task in browser to enter / leave fullscreen
 * @param browser - Browser to use for JS fullscreen requests
 * @param {Boolean} fullscreenState - true to enter fullscreen, false to leave
 * @returns {Promise} - Resolves once fullscreen change is applied
 */
function changeFullscreen(browser, fullScreenState) {
  let fullScreenChange = waitForFullScreenState(browser, fullScreenState);
  ContentTask.spawn(browser, fullScreenState, state => {
    if (state) {
      content.document.body.requestFullscreen();
    } else {
      content.document.exitFullscreen();
    }
  });
  return fullScreenChange;
}
