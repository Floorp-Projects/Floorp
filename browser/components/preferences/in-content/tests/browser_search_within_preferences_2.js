"use strict";
/**
 * This file contains tests for the Preferences search bar.
 */

/* eslint-disable mozilla/no-cpows-in-tests */

/**
 * Enabling searching functionality. Will display search bar from this testcase forward.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Test that we only search the selected child of a XUL deck.
 * When we search "Cancel Setup",
 * it should not show the "Cancel Setup" button if the Firefox account is not logged in yet.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  // Ensure the "Sign Up" button in the hidden child of the <xul:deck>
  // is selected and displayed on the screen.
  let weavePrefsDeck = gBrowser.contentDocument.getElementById("weavePrefsDeck");
  is(weavePrefsDeck.selectedIndex, 0, "Should select the #noFxaAccount child node");
  let noFxaSignUp = weavePrefsDeck.childNodes[0].querySelector("#noFxaSignUp");
  is(noFxaSignUp.textContent, "Don\u2019t have an account? Get started", "The Sign Up button should exist");

  // Performs search.
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "Don\u2019t have an account? Get started";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (child.id == "header-searchResults" ||
        child.id == "weavePrefsDeck") {
      is_element_visible(child, "Should be in search results");
    } else if (child.id) {
      is_element_hidden(child, "Should not be in search results");
    }
  }

  // Ensure the "Cancel Setup" button exists in the hidden child of the <xul:deck>.
  let unlinkFxaAccount = weavePrefsDeck.childNodes[1].querySelector("#unverifiedUnlinkFxaAccount");
  is(unlinkFxaAccount.label, "Cancel Setup", "The Cancel Setup button should exist");

  // Performs search.
  searchInput.focus();
  query = "Cancel Setup";
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let noResultsEl = gBrowser.contentDocument.querySelector(".no-results-message");
  is_element_visible(noResultsEl, "Should be reporting no results");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
