/* Tests for proper behaviour of "Show this frame" context menu options with a valid frame and
   a frame with an invalid url.
 */

// Two frames, one with text content, the other an error page
var invalidPage = "http://127.0.0.1:55555/";
var validPage = "http://example.com/";
var testPage =
  'data:text/html,<frameset cols="400,400"><frame src="' +
  validPage +
  '"><frame src="' +
  invalidPage +
  '"></frameset>';

async function openTestPage() {
  // Waiting for the error page to load in the subframe
  await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage, true, true);
  await BrowserTestUtils.waitForCondition(() => {
    return (
      gBrowser.selectedBrowser.browsingContext.children[1].currentWindowGlobal
        .documentURI.spec != "about:blank"
    );
  });

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser.browsingContext.children[1],
    [],
    () => {
      if (!content.document.body.classList.length) {
        return new Promise(resolve => {
          content.addEventListener(
            "AboutNetErrorLoad",
            () => {
              resolve();
            },
            { once: true }
          );
        });
      }

      return undefined;
    }
  );
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

  contextMenu.activateItem(document.getElementById(menuId));
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

    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, false);
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
