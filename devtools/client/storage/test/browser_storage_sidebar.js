/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the sidebar opens, closes and updates
// This test is not testing the values in the sidebar, being tested in _values

// Format: [
//   <id of the table item to click> or <id array for tree item to select> or
//     null to press Escape,
//   <do we wait for the async "sidebar-updated" event>,
//   <is the sidebar open>
// ]

"use strict";

const testCases = [
  {
    location: ["cookies", "https://sectest1.example.org"],
    sidebarHidden: true,
  },
  {
    location: getCookieId("cs2", ".example.org", "/"),
    sidebarHidden: false,
  },
  {
    sendEscape: true,
  },
  {
    location: getCookieId("cs2", ".example.org", "/"),
    sidebarHidden: true,
  },
  {
    location: getCookieId("uc1", ".example.org", "/"),
    sidebarHidden: true,
  },
  {
    location: getCookieId("uc1", ".example.org", "/"),
    sidebarHidden: true,
  },

  {
    location: ["localStorage", "http://sectest1.example.org"],
    sidebarHidden: true,
  },
  {
    location: "iframe-u-ls1",
    sidebarHidden: false,
  },
  {
    location: "iframe-u-ls1",
    sidebarHidden: false,
  },
  {
    sendEscape: true,
  },

  {
    location: ["sessionStorage", "http://test1.example.org"],
    sidebarHidden: true,
  },
  {
    location: "ss1",
    sidebarHidden: false,
  },
  {
    sendEscape: true,
  },

  {
    location: ["indexedDB", "http://test1.example.org"],
    sidebarHidden: true,
  },
  {
    location: "idb2 (default)",
    sidebarHidden: false,
  },

  {
    location: [
      "indexedDB",
      "http://test1.example.org",
      "idb2 (default)",
      "obj3",
    ],
    sidebarHidden: true,
  },

  {
    location: ["indexedDB", "https://sectest1.example.org", "idb-s2 (default)"],
    sidebarHidden: true,
  },
  {
    location: "obj-s2",
    sidebarHidden: false,
  },
  {
    sendEscape: true,
  },
  {
    location: "obj-s2",
    sidebarHidden: true,
  },
];

add_task(async function() {
  // storage-listings.html explicitly mixes secure and insecure frames.
  // We should not enforce https for tests using this page.
  await pushPref("dom.security.https_first", false);

  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  for (const test of testCases) {
    const { location, sidebarHidden, sendEscape } = test;

    info("running " + JSON.stringify(test));

    if (Array.isArray(location)) {
      await selectTreeItem(location);
    } else if (location) {
      await selectTableItem(location);
    }

    if (sendEscape) {
      EventUtils.sendKey("ESCAPE", gPanelWindow);
    } else {
      is(
        gUI.sidebar.hidden,
        sidebarHidden,
        "correct visibility state of sidebar."
      );
    }

    info("-".repeat(80));
  }
});
