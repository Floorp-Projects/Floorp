/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

test_newtab({
  before: setDefaultTopSites,
  // Test verifies the menu options for a default top site.
  test: async function defaultTopSites_menuOptions() {
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".top-site-icon"),
      "Topsite tippytop icon not found");

    let contextMenuItems = content.openContextMenuAndGetOptions(".top-sites-list li:first-child").map(v => v.textContent);

    Assert.equal(contextMenuItems.length, 5, "Number of options is correct");

    const expectedItemsText = ["Pin", "Edit", "Open in a New Window", "Open in a New Private Window", "Dismiss"];

    for (let i = 0; i < contextMenuItems.length; i++) {
      Assert.equal(contextMenuItems[i], expectedItemsText[i], "Name option is correct");
    }
  }
});

test_newtab({
  before: setDefaultTopSites,
  // Test verifies that the next top site in queue replaces a dismissed top site.
  test: async function defaultTopSites_dismiss() {
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".top-site-icon"),
      "Topsite tippytop icon not found");

    let defaultTopSitesNumber = content.document.querySelector(".top-sites-list").querySelectorAll("[class=\"top-site-outer\"]").length;
    Assert.equal(defaultTopSitesNumber, 6, "6 top sites are loaded by default");

    let secondTopSite = content.document.querySelector(".top-sites-list li:nth-child(2) a").getAttribute("href");

    let contextMenuItems = content.openContextMenuAndGetOptions(".top-sites-list li:first-child");
    Assert.equal(contextMenuItems[4].textContent, "Dismiss", "'Dismiss' is the 5th item in the context menu list");

    contextMenuItems[4].querySelector("a").click();

    // Need to wait for dismiss action.
    await ContentTaskUtils.waitForCondition(() => content.document.querySelector(".top-sites-list li:first-child a").getAttribute("href") === secondTopSite,
      "First topsite was dismissed");

    defaultTopSitesNumber = content.document.querySelector(".top-sites-list").querySelectorAll("[class=\"top-site-outer\"]").length;
    Assert.equal(defaultTopSitesNumber, 5, "5 top sites are displayed after one of them is dismissed");
  },
  async after() {
    await new Promise(resolve => NewTabUtils.undoAll(resolve));
  }
});
