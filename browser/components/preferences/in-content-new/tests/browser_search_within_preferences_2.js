"use strict";
/**
 * This file contains tests for the Preferences search bar.
 */

/**
 * Enabling searching functionality. Will display search bar from this testcase forward.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Test that we only search the selected child of a XUL deck.
 * When we search "Forget this Email",
 * it should not show the "Forget this Email" button if the Firefox account is not logged in yet.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  // Ensure the "Create Account" button in the hidden child of the <xul:deck>
  // is selected and displayed on the screen.
  let weavePrefsDeck = gBrowser.contentDocument.getElementById("weavePrefsDeck");
  is(weavePrefsDeck.selectedIndex, 0, "Should select the #noFxaAccount child node");
  let noFxaSignUp = weavePrefsDeck.childNodes[0].querySelector("#noFxaSignUp");
  is(noFxaSignUp.label, "Create Account", "The Create Account button should exist");

  // Performs search.
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  searchInput.value = "Create Account";
  searchInput.doCommand();

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

  // Ensure the "Forget this Email" button exists in the hidden child of the <xul:deck>.
  let unlinkFxaAccount = weavePrefsDeck.childNodes[1].querySelector("#unverifiedUnlinkFxaAccount");
  is(unlinkFxaAccount.label, "Forget this Email", "The Forget this Email button should exist");

  // Performs search.
  searchInput.focus();
  searchInput.value = "Forget this Email";
  searchInput.doCommand();

  let noResultsEl = gBrowser.contentDocument.querySelector(".no-results-message");
  is_element_visible(noResultsEl, "Should be reporting no results");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
