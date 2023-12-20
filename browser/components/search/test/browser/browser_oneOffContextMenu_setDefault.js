"use strict";

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCHBAR_BASE_ID = "searchbar-engine-one-off-item-";

let originalEngine;
let originalPrivateEngine;

async function resetEngines() {
  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    originalPrivateEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
}

registerCleanupFunction(resetEngines);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
      ["browser.search.widget.inNavBar", true],
    ],
  });
  originalEngine = await Services.search.getDefault();
  originalPrivateEngine = await Services.search.getDefaultPrivate();
  registerCleanupFunction(async () => {
    await resetEngines();
  });

  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
  });
});

async function testSearchBarChangeEngine(win, testPrivate, isPrivateWindow) {
  info(
    `Testing search bar with testPrivate: ${testPrivate} isPrivateWindow: ${isPrivateWindow}`
  );

  const searchPopup = win.document.getElementById("PopupSearchAutoComplete");
  const searchOneOff = searchPopup.oneOffButtons;

  // Ensure the engine is reset.
  await resetEngines();

  let oneOffButton = await openPopupAndGetEngineButton(
    searchPopup,
    searchOneOff,
    SEARCHBAR_BASE_ID,
    TEST_ENGINE_NAME
  );

  const contextMenu = searchOneOff.contextMenuPopup;
  const setDefaultEngineMenuItem = searchOneOff.querySelector(
    ".search-one-offs-context-set-default" + (testPrivate ? "-private" : "")
  );

  // Click the set default engine menu item.
  let promise = promiseDefaultEngineChanged(testPrivate);
  contextMenu.activateItem(setDefaultEngineMenuItem);

  // This also checks the engine correctly changed.
  await promise;

  if (testPrivate == isPrivateWindow) {
    let expectedName = originalEngine.name;
    let expectedImage = originalEngine.getIconURL();
    if (isPrivateWindow) {
      expectedName = originalPrivateEngine.name;
      expectedImage = originalPrivateEngine.getIconURL();
    }

    Assert.equal(
      oneOffButton.getAttribute("tooltiptext"),
      expectedName,
      "Should now have the original engine's name for the tooltip"
    );
    Assert.equal(
      oneOffButton.image,
      expectedImage,
      "Should now have the original engine's uri for the image"
    );
  }

  await promiseClosePopup(searchPopup);
}

add_task(async function test_searchBarChangeEngine() {
  await testSearchBarChangeEngine(window, false, false);
  await testSearchBarChangeEngine(window, true, false);
});

add_task(async function test_searchBarChangeEngine_privateWindow() {
  const win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await testSearchBarChangeEngine(win, true, true);
  await BrowserTestUtils.closeWindow(win);
});

/**
 * Promises that an engine change has happened for the current engine, which
 * has resulted in the test engine now being the current engine.
 *
 * @param {boolean} testPrivate
 *   Set to true if we're testing the private default engine.
 * @returns {Promise} Resolved once the test engine is set as the current engine.
 */
function promiseDefaultEngineChanged(testPrivate) {
  const expectedNotification = testPrivate
    ? "engine-default-private"
    : "engine-default";
  return new Promise(resolve => {
    function observer(aSub, aTopic, aData) {
      if (aData == expectedNotification) {
        Assert.equal(
          Services.search[
            testPrivate ? "defaultPrivateEngine" : "defaultEngine"
          ].name,
          TEST_ENGINE_NAME,
          "defaultEngine set"
        );
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        resolve();
      }
    }

    Services.obs.addObserver(observer, "browser-search-engine-modified");
  });
}

/**
 * Opens the specified search popup and gets the test engine from the
 * one-off buttons.
 *
 * @param {object} popup The expected popup.
 * @param {object} oneOffInstance The expected one-off instance for the popup.
 * @param {string} baseId The expected string for the id of the current
 *                        engine button, without the engine name.
 * @param {string} engineName The engine name for finding the one-off button.
 * @returns {object} Returns an object that represents the one off button for the
 *                          test engine.
 */
async function openPopupAndGetEngineButton(
  popup,
  oneOffInstance,
  baseId,
  engineName
) {
  const win = oneOffInstance.container.ownerGlobal;
  // Open the popup.
  win.gURLBar.blur();
  let shownPromise = promiseEvent(popup, "popupshown");
  let builtPromise = promiseEvent(oneOffInstance, "rebuild");
  let searchbar = win.document.getElementById("searchbar");
  let searchIcon = searchbar.querySelector(".searchbar-search-button");
  // Use the search icon to avoid hitting the network.
  EventUtils.synthesizeMouseAtCenter(searchIcon, {}, win);
  await Promise.all([shownPromise, builtPromise]);

  const contextMenu = oneOffInstance.contextMenuPopup;
  let oneOffButton = oneOffInstance.buttons;

  // Get the one-off button for the test engine.
  for (
    oneOffButton = oneOffButton.firstChild;
    oneOffButton;
    oneOffButton = oneOffButton.nextSibling
  ) {
    if (
      oneOffButton.nodeType == Node.ELEMENT_NODE &&
      oneOffButton.engine &&
      oneOffButton.engine.name == engineName
    ) {
      break;
    }
  }

  Assert.notEqual(
    oneOffButton,
    undefined,
    "One-off for test engine should exist"
  );
  Assert.equal(
    oneOffButton.getAttribute("tooltiptext"),
    engineName,
    "One-off should have the tooltip set to the engine name"
  );

  Assert.ok(
    oneOffButton.id.startsWith(baseId + "engine-"),
    "Should have an appropriate id"
  );

  // Open the context menu on the one-off.
  let promise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    oneOffButton,
    {
      type: "contextmenu",
      button: 2,
    },
    win
  );
  await promise;

  return oneOffButton;
}

/**
 * Closes the popup and moves the mouse away from it.
 *
 * @param {Button} popup The popup to close.
 */
async function promiseClosePopup(popup) {
  // close the panel using the escape key.
  let promise = promiseEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape", {}, popup.ownerGlobal);
  await promise;

  // Move the cursor out of the panel area to avoid messing with other tests.
  EventUtils.synthesizeNativeMouseEvent({
    type: "mousemove",
    target: popup,
    offsetX: 0,
    offsetY: 0,
    win: popup.ownerGlobal,
  });
}
