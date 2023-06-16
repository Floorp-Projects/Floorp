/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.sys.mjs",
});

let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

// |shouldWork| should be true if opensearch is expected to work and false if
// it is not.
async function test_opensearch(shouldWork) {
  let searchBar = BrowserSearch.searchBar;

  let rootDir = getRootDirectory(gTestPath);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    rootDir + "opensearch.html"
  );
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let promiseSearchPopupShown = BrowserTestUtils.waitForEvent(
    searchPopup,
    "popupshown"
  );
  let searchBarButton = searchBar.querySelector(".searchbar-search-button");

  searchBarButton.click();
  await promiseSearchPopupShown;
  let oneOffsContainer = searchPopup.searchOneOffsContainer;
  let engineElement = oneOffsContainer.querySelector(
    ".searchbar-engine-one-off-add-engine"
  );
  if (shouldWork) {
    ok(engineElement, "There should be search engines available to add");
    ok(
      searchBar.getAttribute("addengines"),
      "Search bar should have addengines attribute"
    );
  } else {
    is(
      engineElement,
      null,
      "There should be no search engines available to add"
    );
    ok(
      !searchBar.getAttribute("addengines"),
      "Search bar should not have addengines attribute"
    );
  }
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_opensearch_works() {
  // Clear out policies so we can test with no policies applied
  await setupPolicyEngineWithJson({
    policies: {},
  });
  // Ensure that opensearch works before we make sure that it can be properly
  // disabled
  await test_opensearch(true);
});

add_task(async function setup_prevent_installs() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        PreventInstalls: true,
      },
    },
  });
});

add_task(async function test_prevent_install_ui() {
  // Check that about:preferences does not prompt user to install search engines
  // if that feature is disabled
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#search"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let linkContainer = content.document.getElementById("addEnginesBox");
    if (!linkContainer.hidden) {
      await ContentTaskUtils.waitForMutationCondition(
        linkContainer,
        { attributeFilter: ["hidden"] },
        () => linkContainer.hidden
      );
    }
    ok(
      linkContainer.hidden,
      '"Find more search engines" link should be hidden'
    );
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_opensearch_disabled() {
  // Check that search engines cannot be added via opensearch
  await test_opensearch(false);
});
