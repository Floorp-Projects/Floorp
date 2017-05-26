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
 * When we search "perform", it should show the "performanceGroup".
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  // Ensure the "not default browser" child node of the <xul:deck>
  // is selected and displayed on the screen.
  let setDefaultPane = gBrowser.contentDocument.getElementById("setDefaultPane");
  is(setDefaultPane.selectedIndex, 0, "Should select the not default browser child node");
  let isNotDefaultLabel = setDefaultPane.querySelectorAll("hbox")[0].querySelector("label");
  ok(isNotDefaultLabel.textContent.includes("not your default browser"));

  // Performs search.
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  searchInput.focus();
  searchInput.value = "is not your default browser";
  searchInput.doCommand();

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (child.id == "header-searchResults" ||
        child.id == "startupGroup") {
      is_element_visible(child, "Should be in search results");
    } else if (child.id) {
      is_element_hidden(child, "Should not be in search results");
    }
  }

  // Ensure the default browser label exists in the hidden child of the <xul:deck>.
  let isDefaultLabel = setDefaultPane.querySelectorAll("hbox")[1].querySelector("label");
  ok(isDefaultLabel.textContent.includes("currently your default browser"));

  // Performs search.
  searchInput.focus();
  searchInput.value = "is currently your default browser";
  searchInput.doCommand();

  let noResultsEl = gBrowser.contentDocument.querySelector(".no-results-message");
  is_element_visible(noResultsEl, "Should be reporting no results");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
