/* Tests for proper behaviour of "Show this frame" context menu options with a valid frame and
   a frame with an invalid url.
 */

// Two frames, one with text content, the other an error page
var invalidPage = "http://127.0.0.1:55555/";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
var validPage = "http://example.com/";
var testPage =
  'data:text/html,<frameset cols="400,400"><frame src="' +
  validPage +
  '"><frame src="' +
  invalidPage +
  '"></frameset>';

async function openTestPage() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    true,
    true
  );
  let browser = tab.linkedBrowser;

  // The test page has a top-level document and two subframes. One of
  // those subframes is an error page, which doesn't fire a load event.
  // We'll use BrowserTestUtils.browserLoaded and have it wait for all
  // 3 loads before resolving.
  let expectedLoads = 3;
  let pageAndIframesLoaded = BrowserTestUtils.browserLoaded(
    browser,
    true /* includeSubFrames */,
    url => {
      expectedLoads--;
      return !expectedLoads;
    },
    true /* maybeErrorPage */
  );
  BrowserTestUtils.startLoadingURIString(browser, testPage);
  await pageAndIframesLoaded;

  // Make sure both the top-level document and the iframe documents have
  // had a chance to present. We need this so that the context menu event
  // gets dispatched properly.
  for (let bc of [
    ...browser.browsingContext.children,
    browser.browsingContext,
  ]) {
    await SpecialPowers.spawn(bc, [], async function () {
      await new Promise(resolve => {
        content.requestAnimationFrame(resolve);
      });
    });
  }
}

async function selectFromFrameMenu(frameNumber, menuId) {
  const contextMenu = document.getElementById("contentAreaContextMenu");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  await BrowserTestUtils.synthesizeMouseAtPoint(
    40,
    40,
    {
      type: "contextmenu",
      button: 2,
    },
    gBrowser.selectedBrowser.browsingContext.children[frameNumber]
  );

  await popupShownPromise;

  let frameItem = document.getElementById("frame");
  let framePopup = frameItem.menupopup;
  let subPopupShownPromise = BrowserTestUtils.waitForEvent(
    framePopup,
    "popupshown"
  );

  frameItem.openMenu(true);
  await subPopupShownPromise;

  let subPopupHiddenPromise = BrowserTestUtils.waitForEvent(
    framePopup,
    "popuphidden"
  );
  let contextMenuHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.activateItem(document.getElementById(menuId));
  await subPopupHiddenPromise;
  await contextMenuHiddenPromise;
}

add_task(async function testOpenFrame() {
  for (let frameNumber = 0; frameNumber < 2; frameNumber++) {
    await openTestPage();

    let expectedResultURI = [validPage, invalidPage][frameNumber];

    info("show only this frame for " + expectedResultURI);

    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      expectedResultURI,
      frameNumber == 1
    );

    await selectFromFrameMenu(frameNumber, "context-showonlythisframe");
    await browserLoadedPromise;

    is(
      gBrowser.selectedBrowser.currentURI.spec,
      expectedResultURI,
      "Should navigate to page url, not about:neterror"
    );

    gBrowser.removeCurrentTab();
  }
});

add_task(async function testOpenFrameInTab() {
  for (let frameNumber = 0; frameNumber < 2; frameNumber++) {
    await openTestPage();

    let expectedResultURI = [validPage, invalidPage][frameNumber];

    info("open frame in tab for " + expectedResultURI);

    let newTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      expectedResultURI,
      false
    );
    await selectFromFrameMenu(frameNumber, "context-openframeintab");
    let newTab = await newTabPromise;

    await BrowserTestUtils.switchTab(gBrowser, newTab);

    // We should now have the error page in a new, active tab.
    is(
      gBrowser.selectedBrowser.currentURI.spec,
      expectedResultURI,
      "New tab should have page url, not about:neterror"
    );

    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();
  }
});

add_task(async function testOpenFrameInWindow() {
  for (let frameNumber = 0; frameNumber < 2; frameNumber++) {
    await openTestPage();

    let expectedResultURI = [validPage, invalidPage][frameNumber];

    info("open frame in window for " + expectedResultURI);

    let newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: frameNumber == 1 ? invalidPage : validPage,
      maybeErrorPage: frameNumber == 1,
    });
    await selectFromFrameMenu(frameNumber, "context-openframe");
    let newWindow = await newWindowPromise;

    is(
      newWindow.gBrowser.selectedBrowser.currentURI.spec,
      expectedResultURI,
      "New window should have page url, not about:neterror"
    );

    newWindow.close();
    gBrowser.removeCurrentTab();
  }
});
