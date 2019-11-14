"use strict";
/**
 * This file contains tests for the Preferences search bar.
 */

/**
 * Enabling searching functionality. Will display search bar from this testcase forward.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.search", true]],
  });
});

/**
 * Test that we only search the selected child of a XUL deck.
 * When we search "Remove Account",
 * it should not show the "Remove Account" button if the Firefox account is not logged in yet.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneSync", { leaveOpen: true });

  let weavePrefsDeck = gBrowser.contentDocument.getElementById(
    "weavePrefsDeck"
  );
  is(
    weavePrefsDeck.selectedIndex,
    0,
    "Should select the #noFxaAccount child node"
  );

  // Performs search.
  let searchInput = gBrowser.contentDocument.getElementById("searchInput");

  is(
    searchInput,
    gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences"
  );

  let query = "Sync";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow,
    "PreferencesSearchCompleted",
    evt => evt.detail == query
  );
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (child.id == "header-searchResults" || child.id == "weavePrefsDeck") {
      is_element_visible(child, "Should be in search results");
    } else if (child.id) {
      is_element_hidden(child, "Should not be in search results");
    }
  }

  // Ensure the "Remove Account" button exists in the hidden child of the <xul:deck>.
  let unlinkFxaAccount = weavePrefsDeck.children[1].querySelector(
    "#unverifiedUnlinkFxaAccount"
  );
  is(
    unlinkFxaAccount.label,
    "Remove Account",
    "The Remove Account button should exist"
  );

  // Performs search.
  searchInput.focus();
  query = "Remove Account";
  searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow,
    "PreferencesSearchCompleted",
    evt => evt.detail == query
  );
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let noResultsEl = gBrowser.contentDocument.querySelector(
    "#no-results-message"
  );
  is_element_visible(noResultsEl, "Should be reporting no results");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test that we search using `search-l10n-ids`.
 *
 * The test uses element `showUpdateHistory` and
 * l10n id `language-and-appearance-header` and expects the element
 * to be matched on the first word from the l10n id value ("Language" in en-US).
 */
add_task(async function() {
  let l10nId = "language-and-appearance-header";

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  // First, lets make sure that the element is not matched without
  // `search-l10n-ids`.
  {
    let searchInput = gBrowser.contentDocument.getElementById("searchInput");
    let suhElem = gBrowser.contentDocument.getElementById("showUpdateHistory");

    is(
      searchInput,
      gBrowser.contentDocument.activeElement.closest("#searchInput"),
      "Search input should be focused when visiting preferences"
    );

    ok(
      !suhElem.getAttribute("search-l10n-ids").includes(l10nId),
      "showUpdateHistory element should not contain the l10n id here."
    );

    let query = "Language";
    let searchCompletedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.contentWindow,
      "PreferencesSearchCompleted",
      evt => evt.detail == query
    );
    EventUtils.sendString(query);
    await searchCompletedPromise;

    is_element_hidden(
      suhElem,
      "showUpdateHistory should not be in search results"
    );
  }

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Now, let's add the l10n id to the element and perform the same search again.

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  {
    let searchInput = gBrowser.contentDocument.getElementById("searchInput");

    is(
      searchInput,
      gBrowser.contentDocument.activeElement.closest("#searchInput"),
      "Search input should be focused when visiting preferences"
    );

    let suhElem = gBrowser.contentDocument.getElementById("showUpdateHistory");
    suhElem.setAttribute("search-l10n-ids", l10nId);

    let query = "Language";
    let searchCompletedPromise = BrowserTestUtils.waitForEvent(
      gBrowser.contentWindow,
      "PreferencesSearchCompleted",
      evt => evt.detail == query
    );
    EventUtils.sendString(query);
    await searchCompletedPromise;

    is_element_visible(
      suhElem,
      "showUpdateHistory should be in search results"
    );
  }

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
