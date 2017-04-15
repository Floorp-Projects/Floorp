"use strict";

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCHBAR_BASE_ID = "searchbar-engine-one-off-item-";
const URLBAR_BASE_ID = "urlbar-engine-one-off-item-";
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

const searchbar = document.getElementById("searchbar");
const urlbar = document.getElementById("urlbar");
const searchPopup = document.getElementById("PopupSearchAutoComplete");
const urlbarPopup = document.getElementById("PopupAutoCompleteRichResult");
const searchIcon = document.getAnonymousElementByAttribute(
  searchbar, "anonid", "searchbar-search-button"
);
const searchOneOffBinding = document.getAnonymousElementByAttribute(
  searchPopup, "anonid", "search-one-off-buttons"
);
const urlBarOneOffBinding = document.getAnonymousElementByAttribute(
  urlbarPopup, "anonid", "one-off-search-buttons"
);

let originalEngine = Services.search.currentEngine;

function resetEngine() {
  Services.search.currentEngine = originalEngine;
}

registerCleanupFunction(resetEngine);

add_task(function* init() {
  yield promiseNewEngine(TEST_ENGINE_BASENAME, {
    setAsCurrent: false,
  });
});

add_task(function* test_searchBarChangeEngine() {
  let oneOffButton = yield openPopupAndGetEngineButton(true, searchPopup,
                                                       searchOneOffBinding,
                                                       SEARCHBAR_BASE_ID);

  const setDefaultEngineMenuItem = document.getAnonymousElementByAttribute(
    searchOneOffBinding, "anonid", "search-one-offs-context-set-default"
  );

  // Click the set default engine menu item.
  let promise = promiseCurrentEngineChanged();
  EventUtils.synthesizeMouseAtCenter(setDefaultEngineMenuItem, {});

  // This also checks the engine correctly changed.
  yield promise;

  Assert.equal(oneOffButton.id, SEARCHBAR_BASE_ID + originalEngine.name,
               "Should now have the original engine's id for the button");
  Assert.equal(oneOffButton.getAttribute("tooltiptext"), originalEngine.name,
               "Should now have the original engine's name for the tooltip");
  Assert.equal(oneOffButton.image, originalEngine.iconURI.spec,
               "Should now have the original engine's uri for the image");

  yield promiseClosePopup(searchPopup);
});

add_task(function* test_urlBarChangeEngine() {
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);
  registerCleanupFunction(function* () {
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
  });

  // Ensure the engine is reset.
  resetEngine();

  let oneOffButton = yield openPopupAndGetEngineButton(false, urlbarPopup,
                                                       urlBarOneOffBinding,
                                                       URLBAR_BASE_ID);

  const setDefaultEngineMenuItem = document.getAnonymousElementByAttribute(
    urlBarOneOffBinding, "anonid", "search-one-offs-context-set-default"
  );

  // Click the set default engine menu item.
  let promise = promiseCurrentEngineChanged();
  EventUtils.synthesizeMouseAtCenter(setDefaultEngineMenuItem, {});

  // This also checks the engine correctly changed.
  yield promise;

  let currentEngine = Services.search.currentEngine;

  // For the urlbar, we should keep the new engine's icon.
  Assert.equal(oneOffButton.id, URLBAR_BASE_ID + currentEngine.name,
               "Should now have the original engine's id for the button");
  Assert.equal(oneOffButton.getAttribute("tooltiptext"), currentEngine.name,
               "Should now have the original engine's name for the tooltip");
  Assert.equal(oneOffButton.image, currentEngine.iconURI.spec,
               "Should now have the original engine's uri for the image");

  yield promiseClosePopup(urlbarPopup);
});

/**
 * Promises that an engine change has happened for the current engine, which
 * has resulted in the test engine now being the current engine.
 *
 * @return {Promise} Resolved once the test engine is set as the current engine.
 */
function promiseCurrentEngineChanged() {
  return new Promise(resolve => {
    function observer(aSub, aTopic, aData) {
      if (aData == "engine-current") {
        Assert.ok(Services.search.currentEngine.name, TEST_ENGINE_NAME, "currentEngine set");
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
 * @param {Boolean} isSearch true if the search popup should be opened; false
 *                           for the urlbar popup.
 * @param {Object} popup The expected popup.
 * @param {Object} oneOffBinding The expected one-off-binding for the popup.
 * @param {String} baseId The expected string for the id of the current
 *                        engine button, without the engine name.
 * @return {Object} Returns an object that represents the one off button for the
 *                          test engine.
 */
function* openPopupAndGetEngineButton(isSearch, popup, oneOffBinding, baseId) {
  // Open the popup.
  let promise = promiseEvent(popup, "popupshown");
  info("Opening panel");

  // We have to open the popups in differnt ways.
  if (isSearch) {
    // Use the search icon to avoid hitting the network.
    EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  } else {
    // There's no history at this stage, so we need to press a key.
    urlbar.focus();
    EventUtils.synthesizeKey("a", {});
  }
  yield promise;

  const contextMenu = document.getAnonymousElementByAttribute(
    oneOffBinding, "anonid", "search-one-offs-context-menu"
  );
  const oneOffButtons = document.getAnonymousElementByAttribute(
    oneOffBinding, "anonid", "search-panel-one-offs"
  );

  // Get the one-off button for the test engine.
  let oneOffButton;
  for (let node of oneOffButtons.childNodes) {
    if (node.engine && node.engine.name == TEST_ENGINE_NAME) {
      oneOffButton = node;
      break;
    }
  }
  Assert.notEqual(oneOffButton, undefined,
                  "One-off for test engine should exist");
  Assert.equal(oneOffButton.getAttribute("tooltiptext"), TEST_ENGINE_NAME,
               "One-off should have the tooltip set to the engine name");
  Assert.equal(oneOffButton.id, baseId + TEST_ENGINE_NAME,
               "Should have the correct id");

  // Open the context menu on the one-off.
  promise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(oneOffButton, {
    type: "contextmenu",
    button: 2,
  });
  yield promise;

  return oneOffButton;
}

/**
 * Closes the popup and moves the mouse away from it.
 *
 * @param {Button} popup The popup to close.
 */
function* promiseClosePopup(popup) {
  // close the panel using the escape key.
  let promise = promiseEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;

  // Move the cursor out of the panel area to avoid messing with other tests.
  yield EventUtils.synthesizeNativeMouseMove(popup);
}
