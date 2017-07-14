"use strict";
/**
 * This file contains tests for the Preferences search bar.
 */

requestLongerTimeout(2);

/**
 * Tests to see if search bar is being hidden when pref is turned off
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", false]]});
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let searchInput = gBrowser.contentDocument.querySelectorAll("#searchInput");
  is(searchInput.length, 1, "There should only be one element name searchInput querySelectorAll");
  is_element_hidden(searchInput[0], "Search box should be hidden");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

/**
 * Enabling searching functionality. Will display search bar from this testcase forward.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Tests to see if search bar is being shown when pref is turned on
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  is_element_visible(searchInput, "Search box should be shown");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for "Search Result" panel.
 * After it runs a search, it tests if the "Search Results" panel is the only selected category.
 * The search is then cleared, it then tests if the "General" panel is the only selected category.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "password";
  searchInput.doCommand();

  let categoriesList = gBrowser.contentDocument.getElementById("categories");

  for (let i = 0; i < categoriesList.childElementCount; i++) {
    let child = categoriesList.children[i]
    if (child.id == "category-search-results") {
      is(child.selected, true, "Search results panel should be selected");
    } else if (child.id) {
      is(child.selected, false, "No other panel should be selected");
    }
  }
  // Takes search off
  searchInput.value = "";
  searchInput.doCommand();

  // Checks if back to generalPane
  for (let i = 0; i < categoriesList.childElementCount; i++) {
    let child = categoriesList.children[i]
    if (child.id == "category-general") {
      is(child.selected, true, "General panel should be selected");
    } else if (child.id) {
      is(child.selected, false, "No other panel should be selected");
    }
  }

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for "password" case. When we search "password", it should show the "passwordGroup"
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "password";
  searchInput.doCommand();

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");

  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i]
    if (child.id == "passwordsGroup" ||
        child.id == "weavePrefsDeck" ||
        child.id == "header-searchResults" ||
        child.id == "certSelection" ||
        child.id == "connectionGroup") {
      is_element_visible(child, "Should be in search results");
    } else if (child.id) {
      is_element_hidden(child, "Should not be in search results");
    }
  }

  // Takes search off
  searchInput.value = "";
  searchInput.doCommand();

  // Checks if back to generalPane
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i]
    if (child.id == "startupGroup"
    || child.id == "defaultEngineGroup"
    || child.id == "oneClickSearchProvidersGroup"
    || child.id == "paneGeneral"
    || child.id == "accessibilityGroup"
    || child.id == "languagesGroup"
    || child.id == "fontsGroup"
    || child.id == "browsingGroup"
    || child.id == "performanceGroup"
    || child.id == "header-general") {
      is_element_visible(child, "Should be in general tab");
    } else if (child.id) {
      is_element_hidden(child, "Should not be in general tab");
    }
  }

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for if nothing is found
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let noResultsEl = gBrowser.contentDocument.querySelector(".no-results-message");

  is_element_hidden(noResultsEl, "Should not be in search results yet");

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "coach";
  searchInput.doCommand();

  is_element_visible(noResultsEl, "Should be in search results");

  // Takes search off
  searchInput.value = "";
  searchInput.doCommand();

  is_element_hidden(noResultsEl, "Should not be in search results");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for if we go back to general tab after search case
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let generalPane = gBrowser.contentDocument.getElementById("header-general");

  is_element_hidden(generalPane, "Should not be in general");

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "password";
  searchInput.doCommand();

  // Takes search off
  searchInput.value = "";
  searchInput.doCommand();

  // Checks if back to normal
  is_element_visible(generalPane, "Should be in generalPane");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for "Site Data" case, verifying elements with data-hidden-from-search = true
 * are hidden in search result.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.storageManager.enabled", false]]});
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let generalPane = gBrowser.contentDocument.getElementById("header-general");

  is_element_hidden(generalPane, "Should not be in general");

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "site data";
  searchInput.doCommand();

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");

  let child = mainPrefTag.querySelector("#siteDataGroup");
  is_element_hidden(child, "Should be hidden in search results");

  // Takes search off
  searchInput.value = "";
  searchInput.doCommand();

  // Checks if back to normal
  is_element_visible(generalPane, "Should be in generalPane");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for if we go to another tab after searching
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  let searchResultsCategory = gBrowser.contentDocument.getElementById("category-search-results");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "password";
  searchInput.doCommand();
  is(searchResultsCategory.hidden, false, "search results category should be shown");
  is(searchResultsCategory.selected, true, "search results category should be selected");

  let privacyCategory = gBrowser.contentDocument.getElementById("category-privacy");
  privacyCategory.click();
  is(searchResultsCategory.hidden, true, "search results category should not be shown");
  is(searchInput.value, "", "search input should be empty");
  let categoriesList = gBrowser.contentDocument.getElementById("categories");
  for (let i = 0; i < categoriesList.childElementCount; i++) {
    let child = categoriesList.children[i]
    if (child.id == "category-privacy") {
      is(child.selected, true, "Privacy panel should be selected");
    } else if (child.id) {
      is(child.selected, false, "No other panel should be selected");
    }
  }

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
