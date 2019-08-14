"use strict";

const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCHBAR_BASE_ID = "searchbar-engine-one-off-item-";
const URLBAR_BASE_ID = "urlbar-engine-one-off-item-";
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

const urlbar = document.getElementById("urlbar");
const searchPopup = document.getElementById("PopupSearchAutoComplete");
const searchOneOff = searchPopup.oneOffButtons;
const urlBarOneOff = UrlbarTestUtils.getOneOffSearchButtons(window);

var originalEngine;

async function resetEngine() {
  await Services.search.setDefault(originalEngine);
}

registerCleanupFunction(resetEngine);

let searchIcon;

add_task(async function init() {
  originalEngine = await Services.search.getDefault();
  let searchbar = await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
  searchIcon = searchbar.querySelector(".searchbar-search-button");

  await promiseNewEngine(TEST_ENGINE_BASENAME, {
    setAsCurrent: false,
  });
});

add_task(async function test_searchBarChangeEngine() {
  let oneOffButton = await openPopupAndGetEngineButton(
    true,
    searchPopup,
    searchOneOff,
    SEARCHBAR_BASE_ID
  );

  const setDefaultEngineMenuItem = searchOneOff.querySelector(
    ".search-one-offs-context-set-default"
  );

  // Click the set default engine menu item.
  let promise = promisedefaultEngineChanged();
  EventUtils.synthesizeMouseAtCenter(setDefaultEngineMenuItem, {});

  // This also checks the engine correctly changed.
  await promise;

  Assert.equal(
    oneOffButton.id,
    SEARCHBAR_BASE_ID + originalEngine.name,
    "Should now have the original engine's id for the button"
  );
  Assert.equal(
    oneOffButton.getAttribute("tooltiptext"),
    originalEngine.name,
    "Should now have the original engine's name for the tooltip"
  );
  Assert.equal(
    oneOffButton.image,
    originalEngine.iconURI.spec,
    "Should now have the original engine's uri for the image"
  );

  await promiseClosePopup(searchPopup);
});

add_task(async function test_urlBarChangeEngine() {
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
  });

  // Ensure the engine is reset.
  resetEngine();

  let oneOffButton = await openPopupAndGetEngineButton(
    false,
    null,
    urlBarOneOff,
    URLBAR_BASE_ID
  );

  const setDefaultEngineMenuItem = urlBarOneOff.querySelector(
    ".search-one-offs-context-set-default"
  );

  // Click the set default engine menu item.
  let promise = promisedefaultEngineChanged();
  EventUtils.synthesizeMouseAtCenter(setDefaultEngineMenuItem, {});

  // This also checks the engine correctly changed.
  await promise;

  let defaultEngine = await Services.search.getDefault();

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

  await UrlbarTestUtils.promisePopupClose(window);

  // Move the cursor out of the panel area to avoid messing with other tests.
  await EventUtils.synthesizeNativeMouseMove(urlbar);
});

/**
 * Promises that an engine change has happened for the current engine, which
 * has resulted in the test engine now being the current engine.
 *
 * @returns {Promise} Resolved once the test engine is set as the current engine.
 */
function promisedefaultEngineChanged() {
  return new Promise(resolve => {
    function observer(aSub, aTopic, aData) {
      if (aData == "engine-default") {
        Assert.equal(
          Services.search.defaultEngine.name,
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
 * @returns {object} Returns an object that represents the one off button for the
 *                          test engine.
 */
async function openPopupAndGetEngineButton(
  isSearch,
  popup,
  oneOffInstance,
  baseId
) {
  info("Opening panel");

  // We have to open the popups in differnt ways.
  if (isSearch) {
    // Open the popup.
    let promise = promiseEvent(popup, "popupshown");
    // Use the search icon to avoid hitting the network.
    EventUtils.synthesizeMouseAtCenter(searchIcon, {});
    await promise;
  } else {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
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
      oneOffButton.engine.name == TEST_ENGINE_NAME
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
    TEST_ENGINE_NAME,
    "One-off should have the tooltip set to the engine name"
  );
  Assert.equal(
    oneOffButton.id,
    baseId + TEST_ENGINE_NAME,
    "Should have the correct id"
  );

  // Open the context menu on the one-off.
  let promise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(oneOffButton, {
    type: "contextmenu",
    button: 2,
  });
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
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;

  // Move the cursor out of the panel area to avoid messing with other tests.
  await EventUtils.synthesizeNativeMouseMove(popup);
}
