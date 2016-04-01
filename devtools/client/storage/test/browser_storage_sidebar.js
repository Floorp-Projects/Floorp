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
    location: ["cookies", "sectest1.example.org"],
    sidebarHidden: true
  },
  {
    location: "cs2",
    sidebarHidden: false
  },
  {
    sendEscape: true
  },
  {
    location: "cs2",
    sidebarHidden: false
  },
  {
    location: "uc1",
    sidebarHidden: false
  },
  {
    location: "uc1",
    sidebarHidden: false
  },

  {
    location: ["localStorage", "http://sectest1.example.org"],
    sidebarHidden: true
  },
  {
    location: "iframe-u-ls1",
    sidebarHidden: false
  },
  {
    location: "iframe-u-ls1",
    sidebarHidden: false
  },
  {
    sendEscape: true
  },

  {
    location: ["sessionStorage", "http://test1.example.org"],
    sidebarHidden: true
  },
  {
    location: "ss1",
    sidebarHidden: false
  },
  {
    sendEscape: true
  },

  {
    location: ["indexedDB", "http://test1.example.org"],
    sidebarHidden: true
  },
  {
    location: "idb2",
    sidebarHidden: false
  },

  {
    location: ["indexedDB", "http://test1.example.org", "idb2", "obj3"],
    sidebarHidden: true
  },

  {
    location: ["indexedDB", "https://sectest1.example.org", "idb-s2"],
    sidebarHidden: true
  },
  {
    location: "obj-s2",
    sidebarHidden: false
  },
  {
    sendEscape: true
  }, {
    location: "obj-s2",
    sidebarHidden: false
  }
];

add_task(function*() {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  for (let test of testCases) {
    let { location, sidebarHidden, sendEscape } = test;

    info("running " + JSON.stringify(test));

    if (Array.isArray(location)) {
      yield selectTreeItem(location);
    } else if (location) {
      yield selectTableItem(location);
    }

    if (sendEscape) {
      EventUtils.sendKey("ESCAPE", gPanelWindow);
    } else {
      is(gUI.sidebar.hidden, sidebarHidden,
        "correct visibility state of sidebar.");
    }

    info("-".repeat(80));
  }

  yield finishTests();
});
