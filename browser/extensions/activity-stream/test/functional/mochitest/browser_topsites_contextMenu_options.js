/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test verifies the menu options for a default top site.
 */

test_newtab({
  before: setDefaultTopSites,
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
