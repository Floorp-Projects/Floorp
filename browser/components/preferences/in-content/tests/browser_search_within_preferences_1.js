"use strict";
/**
 * This file contains tests for the Preferences search bar.
 */

requestLongerTimeout(6);

/**
 * Tests to see if search bar is being shown when pref is turned on
 */
add_task(async function show_search_bar_when_pref_is_enabled() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", { leaveOpen: true });
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  is_element_visible(searchInput, "Search box should be shown");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for "Search Result" panel.
 * After it runs a search, it tests if the "Search Results" panel is the only selected category.
 * The search is then cleared, it then tests if the "General" panel is the only selected category.
 */
add_task(async function show_search_results_pane_only_then_revert_to_general() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", { leaveOpen: true });

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "password";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let categoriesList = gBrowser.contentDocument.getElementById("categories");

  for (let i = 0; i < categoriesList.childElementCount; i++) {
    let child = categoriesList.children[i];
    is(child.selected, false, "No other panel should be selected");
  }
  // Takes search off
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == "");
  let count = query.length;
  while (count--) {
    EventUtils.sendKey("BACK_SPACE");
  }
  await searchCompletedPromise;

  // Checks if back to generalPane
  for (let i = 0; i < categoriesList.childElementCount; i++) {
    let child = categoriesList.children[i];
    if (child.id == "category-general") {
      is(child.selected, true, "General panel should be selected");
    } else if (child.id) {
      is(child.selected, false, "No other panel should be selected");
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for "password" case. When we search "password", it should show the "passwordGroup"
 */
add_task(async function search_for_password_show_passwordGroup() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", { leaveOpen: true });

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "password";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");

  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
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
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == "");
  let count = query.length;
  while (count--) {
    EventUtils.sendKey("BACK_SPACE");
  }
  await searchCompletedPromise;

  // Checks if back to generalPane
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (child.id == "paneGeneral"
      || child.id == "startupGroup"
      || child.id == "languagesGroup"
      || child.id == "fontsGroup"
      || child.id == "downloadsGroup"
      || child.id == "applicationsGroup"
      || child.id == "drmGroup"
      || child.id == "updateApp"
      || child.id == "browsingGroup"
      || child.id == "performanceGroup"
      || child.id == "connectionGroup"
      || child.id == "generalCategory"
      || child.id == "languageAndAppearanceCategory"
      || child.id == "filesAndApplicationsCategory"
      || child.id == "updatesCategory"
      || child.id == "performanceCategory"
      || child.id == "browsingCategory"
      || child.id == "networkProxyCategory") {
      is_element_visible(child, "Should be in general tab");
    } else if (child.id) {
      is_element_hidden(child, `Should not be in general tab: ${child.id}`);
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for if nothing is found
 */
add_task(async function search_with_nothing_found() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", { leaveOpen: true });

  let noResultsEl = gBrowser.contentDocument.querySelector("#no-results-message");
  let sorryMsgQueryEl = gBrowser.contentDocument.getElementById("sorry-message-query");

  is_element_hidden(noResultsEl, "Should not be in search results yet");

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "coach";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  is_element_visible(noResultsEl, "Should be in search results");
  is(sorryMsgQueryEl.textContent, query, "sorry-message-query should contain the query");

  // Takes search off
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == "");
  let count = query.length;
  while (count--) {
    EventUtils.sendKey("BACK_SPACE");
  }
  await searchCompletedPromise;

  is_element_hidden(noResultsEl, "Should not be in search results");
  is(sorryMsgQueryEl.textContent.length, 0, "sorry-message-query should be empty");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for if we go back to general tab after search case
 */
add_task(async function exiting_search_reverts_to_general_pane() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let generalPane = gBrowser.contentDocument.getElementById("generalCategory");

  is_element_hidden(generalPane, "Should not be in general");

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "password";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  // Takes search off
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == "");
  let count = query.length;
  while (count--) {
    EventUtils.sendKey("BACK_SPACE");
  }
  await searchCompletedPromise;

  // Checks if back to normal
  is_element_visible(generalPane, "Should be in generalPane");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for if we go to another tab after searching
 */
add_task(async function changing_tabs_after_searching() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", { leaveOpen: true });
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "password";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let privacyCategory = gBrowser.contentDocument.getElementById("category-privacy");
  privacyCategory.click();
  is(searchInput.value, "", "search input should be empty");
  let categoriesList = gBrowser.contentDocument.getElementById("categories");
  for (let i = 0; i < categoriesList.childElementCount; i++) {
    let child = categoriesList.children[i];
    if (child.id == "category-privacy") {
      is(child.selected, true, "Privacy panel should be selected");
    } else if (child.id) {
      is(child.selected, false, "No other panel should be selected");
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
