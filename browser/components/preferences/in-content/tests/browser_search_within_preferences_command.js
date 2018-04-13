"use strict";

/**
 * Test for "command" event on search input (when user clicks the x button)
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let generalPane = gBrowser.contentDocument.getElementById("generalCategory");

  is_element_hidden(generalPane, "Should not be in general");

  // Performs search
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  is(searchInput, gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences");

  let query = "x";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == query);
  EventUtils.sendString(query);
  await searchCompletedPromise;

  is_element_hidden(generalPane, "Should not be in generalPane");

  // Takes search off with "command"
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == "");
  searchInput.value = "";
  searchInput.doCommand();
  await searchCompletedPromise;

  // Checks if back to normal
  is_element_visible(generalPane, "Should be in generalPane");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
