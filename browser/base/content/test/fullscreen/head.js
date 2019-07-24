const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { ContentTask } = ChromeUtils.import(
  "resource://testing-common/ContentTask.jsm"
);

function waitForFullScreenState(browser, state) {
  return new Promise(resolve => {
    let eventReceived = false;
    window.messageManager.addMessageListener(
      "DOMFullscreen:Painted",
      function listener() {
        if (!eventReceived) {
          return;
        }
        window.messageManager.removeMessageListener(
          "DOMFullscreen:Painted",
          listener
        );
        resolve();
      }
    );
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
  ContentTask.spawn(browser, fullScreenState, async state => {
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
