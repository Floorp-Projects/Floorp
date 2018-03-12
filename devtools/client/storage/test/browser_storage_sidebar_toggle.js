/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the sidebar toggles when the toggle button is clicked.

"use strict";

const testCases = [
  {
    location: ["cookies", "https://sectest1.example.org"],
    sidebarHidden: true,
    toggleButtonVisible: false
  },
  {
    location: getCookieId("cs2", ".example.org", "/"),
    sidebarHidden: false,
    toggleButtonVisible: true
  },
  {
    clickToggle: true
  },
  {
    location: getCookieId("cs2", ".example.org", "/"),
    sidebarHidden: true
  }
];

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  for (let test of testCases) {
    let { location, sidebarHidden, clickToggle, toggleButtonVisible } = test;

    info("running " + JSON.stringify(test));

    if (Array.isArray(location)) {
      yield selectTreeItem(location);
    } else if (location) {
      yield selectTableItem(location);
    }

    if (clickToggle) {
      toggleSidebar();
    } else if (typeof toggleButtonHidden !== "undefined") {
      is(sidebarToggleVisible(), toggleButtonVisible,
         "correct visibility state of toggle button");
    } else {
      is(gUI.sidebar.hidden, sidebarHidden,
        "correct visibility state of sidebar.");
    }

    info("-".repeat(80));
  }

  yield finishTests();
});
