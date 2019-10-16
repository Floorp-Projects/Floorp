"use strict";

const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCHBAR_BASE_ID = "searchbar-engine-one-off-item-";
const URLBAR_BASE_ID = "urlbar-engine-one-off-item-";
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

let originalEngine;
let originalPrivateEngine;

async function resetEngines() {
  await Services.search.setDefault(originalEngine);
  await Services.search.setDefaultPrivate(originalPrivateEngine);
}

registerCleanupFunction(resetEngines);

add_task(async function init() {
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

  await promiseNewEngine(TEST_ENGINE_BASENAME, {
    setAsCurrent: false,
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
    true,
    searchPopup,
    searchOneOff,
    SEARCHBAR_BASE_ID,
    TEST_ENGINE_NAME
  );

  const setDefaultEngineMenuItem = searchOneOff.querySelector(
    ".search-one-offs-context-set-default" + (testPrivate ? "-private" : "")
  );

  // Click the set default engine menu item.
  let promise = promiseDefaultEngineChanged(testPrivate);
  EventUtils.synthesizeMouseAtCenter(setDefaultEngineMenuItem, {}, win);

  // This also checks the engine correctly changed.
  await promise;

  if (testPrivate == isPrivateWindow) {
    let expectedName = originalEngine.name;
    let expectedImage = originalEngine.iconURI.spec;
    if (isPrivateWindow) {
      expectedName = originalPrivateEngine.name;
      expectedImage = originalPrivateEngine.iconURI.spec;
    }

    Assert.equal(
      oneOffButton.id,
      SEARCHBAR_BASE_ID + expectedName,
      "Should now have the original engine's id for the button"
    );
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

async function testUrlBarChangeEngine(win, testPrivate, isPrivateWindow) {
  info(
    `Testing urlbar with testPrivate: ${testPrivate} isPrivateWindow: ${isPrivateWindow}`
  );
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
  });

  // Ensure the engine is reset.
  await resetEngines();

  const urlbar = win.document.getElementById("urlbar");
  const urlBarOneOff = UrlbarTestUtils.getOneOffSearchButtons(win);

  let oneOffButton = await openPopupAndGetEngineButton(
    false,
    null,
    urlBarOneOff,
    URLBAR_BASE_ID,
    TEST_ENGINE_NAME
  );

  const setDefaultEngineMenuItem = urlBarOneOff.querySelector(
    ".search-one-offs-context-set-default" + (testPrivate ? "-private" : "")
  );

  // Click the set default engine menu item.
  let promise = promiseDefaultEngineChanged(testPrivate);
  EventUtils.synthesizeMouseAtCenter(setDefaultEngineMenuItem, {}, win);

  // This also checks the engine correctly changed.
  await promise;

  let defaultEngine = await Services.search[
    testPrivate ? "getDefaultPrivate" : "getDefault"
  ]();

  // For the urlbar, we should keep the new engine's icon.
  Assert.equal(
    oneOffButton.id,
    URLBAR_BASE_ID + defaultEngine.name,
    "Should now have the original engine's id for the button"
  );
  Assert.equal(
    oneOffButton.getAttribute("tooltiptext"),
    defaultEngine.name,
    "Should now have the original engine's name for the tooltip"
  );
  Assert.equal(
    oneOffButton.image,
    defaultEngine.iconURI.spec,
    "Should now have the original engine's uri for the image"
  );

  await UrlbarTestUtils.promisePopupClose(win);

  // Move the cursor out of the panel area to avoid messing with other tests.
  await EventUtils.synthesizeNativeMouseMove(urlbar);
}

add_task(async function test_urlBarChangeEngine_normal() {
  await testUrlBarChangeEngine(window, false, false);
  await testUrlBarChangeEngine(window, true, false);
});

add_task(async function test_urlBarChangeEngine_private() {
  const win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await testUrlBarChangeEngine(win, true, true);
  await BrowserTestUtils.closeWindow(win);
});

async function testUrlbarEngineDefaultDisabled(isPrivate) {
  const originalDefault = await Services.search[
    isPrivate ? "getDefaultPrivate" : "getDefault"
  ]();

  const urlBarOneOff = UrlbarTestUtils.getOneOffSearchButtons(window);

  const oneOffButton = await openPopupAndGetEngineButton(
    false,
    null,
    urlBarOneOff,
    URLBAR_BASE_ID,
    originalDefault.name
  );

  Assert.equal(
    oneOffButton.id,
    URLBAR_BASE_ID + originalDefault.name,
    "Should now have the original engine's id for the button"
  );

  const setDefaultEngineMenuItem = urlBarOneOff.querySelector(
    ".search-one-offs-context-set-default" + (isPrivate ? "-private" : "")
  );
  Assert.equal(
    setDefaultEngineMenuItem.disabled,
    true,
    "Should have disabled the setting as default for the default engine"
  );

  await UrlbarTestUtils.promisePopupClose(window);
}

add_task(async function test_urlBarEngineDefaultDisabled_normal() {
  await testUrlbarEngineDefaultDisabled(false);
});

add_task(async function test_urlBarEngineDefaultDisabled_private() {
  await testUrlbarEngineDefaultDisabled(true);
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
 * Opens the specified urlbar/search popup and gets the test engine from the
 * one-off buttons.
 *
 * @param {boolean} isSearch true if the search popup should be opened; false
 *                           for the urlbar popup.
 * @param {object} popup The expected popup.
 * @param {object} oneOffInstance The expected one-off instance for the popup.
 * @param {string} baseId The expected string for the id of the current
 *                        engine button, without the engine name.
 * @param {string} engineName The engine name for finding the one-off button.
 * @returns {object} Returns an object that represents the one off button for the
 *                          test engine.
 */
async function openPopupAndGetEngineButton(
  isSearch,
  popup,
  oneOffInstance,
  baseId,
  engineName
) {
  const win = oneOffInstance.container.ownerGlobal;
  // We have to open the popups in differnt ways.
  if (isSearch) {
    // Open the popup.
    win.gURLBar.blur();
    let shownPromise = promiseEvent(popup, "popupshown");
    let builtPromise = promiseEvent(oneOffInstance, "rebuild");
    let searchbar = win.document.getElementById("searchbar");
    let searchIcon = searchbar.querySelector(".searchbar-search-button");
    // Use the search icon to avoid hitting the network.
    EventUtils.synthesizeMouseAtCenter(searchIcon, {}, win);
    await Promise.all([shownPromise, builtPromise]);
  } else {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      waitForFocus,
      value: "a",
    });
  }

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
  Assert.equal(
    oneOffButton.id,
    baseId + engineName,
    "Should have the correct id"
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
  await EventUtils.synthesizeNativeMouseMove(
    popup,
    undefined,
    undefined,
    undefined,
    popup.ownerGlobal
  );
}
