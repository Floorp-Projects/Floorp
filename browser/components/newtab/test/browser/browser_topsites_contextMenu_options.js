/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

test_newtab({
  before: setDefaultTopSites,
  // Test verifies the menu options for a default top site.
  test: async function defaultTopSites_menuOptions() {
    const siteSelector = ".top-site-outer:not(.search-shortcut):not(.placeholder)";
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(siteSelector),
      "Topsite tippytop icon not found");

    const contextMenuItems = content.openContextMenuAndGetOptions(siteSelector).map(v => v.textContent);
    Assert.equal(contextMenuItems.length, 5, "Number of options is correct");

    const expectedItemsText = ["Pin", "Edit", "Open in a New Window", "Open in a New Private Window", "Dismiss"];

    for (let i = 0; i < contextMenuItems.length; i++) {
      Assert.equal(contextMenuItems[i], expectedItemsText[i], "Name option is correct");
    }
  },
});

test_newtab({
  before: setDefaultTopSites,
  // Test verifies that the next top site in queue replaces a dismissed top site.
  test: async function defaultTopSites_dismiss() {
    const siteSelector = ".top-site-outer:not(.search-shortcut):not(.placeholder)";
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(siteSelector),
      "Topsite tippytop icon not found");

    // Don't count search topsites
    const defaultTopSitesNumber = content.document.querySelectorAll(siteSelector).length;
    Assert.equal(defaultTopSitesNumber, 5, "5 top sites are loaded by default");

    // Skip the search topsites select the second default topsite
    const secondTopSite = content.document.querySelectorAll(siteSelector)[1].getAttribute("href");

    const contextMenuItems = content.openContextMenuAndGetOptions(siteSelector);
    Assert.equal(contextMenuItems[4].textContent, "Dismiss", "'Dismiss' is the 5th item in the context menu list");

    contextMenuItems[4].querySelector("a").click();

    // Wait for the topsite to be dismissed and the second one to replace it
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(siteSelector).getAttribute("href") === secondTopSite,
      "First default topsite was dismissed");

    await ContentTaskUtils.waitForCondition(() => content.document.querySelectorAll(siteSelector).length === 4, "4 top sites are displayed after one of them is dismissed");
  },
  async after() {
    await new Promise(resolve => NewTabUtils.undoAll(resolve));
  },
});

test_newtab({
  before: setDefaultTopSites,
  test: async function searchTopSites_dismiss() {
    const siteSelector = ".search-shortcut";
    await ContentTaskUtils.waitForCondition(() => content.document.querySelectorAll(siteSelector).length === 1,
      "1 search topsites is loaded by default");

    const contextMenuItems = content.openContextMenuAndGetOptions(siteSelector);
    is(contextMenuItems.length, 2, "Search TopSites should only have Unpin and Dismiss");

    // Unpin
    contextMenuItems[0].querySelector("a").click();

    await ContentTaskUtils.waitForCondition(() => content.document.querySelectorAll(siteSelector).length === 1,
      "1 search topsite displayed after we unpin the other one");
  },
  after: () => {
    // Required for multiple test runs in the same browser, pref is used to
    // prevent pinning the same search topsite twice
    Services.prefs.clearUserPref("browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts.havePinned");
  },
});
