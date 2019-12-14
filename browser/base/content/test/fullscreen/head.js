const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
function waitForFullScreenState(browser, state) {
  info("inside waitforfullscreenstate");
  return new Promise(resolve => {
    let eventReceived = false;

    let observe = (subject, topic, data) => {
      if (!eventReceived) {
        return;
      }
      Services.obs.removeObserver(observe, "fullscreen-painted");
      resolve();
    };
    Services.obs.addObserver(observe, "fullscreen-painted");

    window.addEventListener(
      `MozDOMFullscreen:${state ? "Entered" : "Exited"}`,
      () => {
        eventReceived = true;
      },
      { once: true }
    );
  });
}

/**
 * Spawns content task in browser to enter / leave fullscreen
 * @param browser - Browser to use for JS fullscreen requests
 * @param {Boolean} fullscreenState - true to enter fullscreen, false to leave
 * @returns {Promise} - Resolves once fullscreen change is applied
 */
async function changeFullscreen(browser, fullScreenState) {
  await new Promise(resolve =>
    SimpleTest.waitForFocus(resolve, browser.ownerGlobal)
  );
  let fullScreenChange = waitForFullScreenState(browser, fullScreenState);
  SpecialPowers.spawn(browser, [fullScreenState], async state => {
    // Wait for document focus before requesting full-screen
    await ContentTaskUtils.waitForCondition(
      () => docShell.isActive && content.document.hasFocus(),
      "Waiting for document focus"
    );
    if (state) {
      content.document.body.requestFullscreen();
    } else {
      content.document.exitFullscreen();
    }
  });
  return fullScreenChange;
}
