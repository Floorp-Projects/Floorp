/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var navbar = document.getElementById("nav-bar");
var navbarCT = navbar.customizationTarget;
var overflowPanelList = document.getElementById("widget-overflow-list");

// Reading currentset
add_task(function() {
  let nodeIds = [];
  for (let node of navbarCT.childNodes) {
    if (node.getAttribute("skipintoolbarset") != "true") {
      nodeIds.push(node.id);
    }
  }
  for (let node of overflowPanelList.childNodes) {
    if (node.getAttribute("skipintoolbarset") != "true") {
      nodeIds.push(node.id);
    }
  }
  let currentSet = navbar.currentSet;
  is(currentSet.split(',').length, nodeIds.length, "Should be just as many nodes as there are.");
  is(currentSet, nodeIds.join(','), "Current set and node IDs should match.");
});

// Insert, then remove items
add_task(function() {
  let currentSet = navbar.currentSet;
  let newCurrentSet = currentSet.replace('home-button', 'feed-button,sync-button,home-button');
  navbar.currentSet = newCurrentSet;
  is(newCurrentSet, navbar.currentSet, "Current set should match expected current set.");
  let feedBtn = document.getElementById("feed-button");
  let syncBtn = document.getElementById("sync-button");
  ok(feedBtn, "Feed button should have been added.");
  ok(syncBtn, "Sync button should have been added.");
  if (feedBtn && syncBtn) {
    let feedParent = feedBtn.parentNode;
    let syncParent = syncBtn.parentNode;
    ok(feedParent == navbarCT || feedParent == overflowPanelList,
       "Feed button should be in navbar or overflow");
    ok(syncParent == navbarCT || syncParent == overflowPanelList,
       "Feed button should be in navbar or overflow");
    is(feedBtn.nextElementSibling, syncBtn, "Feed button should be next to sync button.");
    let homeBtn = document.getElementById("home-button");
    is(syncBtn.nextElementSibling, homeBtn, "Sync button should be next to home button.");
  }
  navbar.currentSet = currentSet;
  is(currentSet, navbar.currentSet, "Should be able to remove the added items.");
});

// Simultaneous insert/remove:
add_task(function() {
  let currentSet = navbar.currentSet;
  let newCurrentSet = currentSet.replace('home-button', 'feed-button');
  navbar.currentSet = newCurrentSet;
  is(newCurrentSet, navbar.currentSet, "Current set should match expected current set.");
  let feedBtn = document.getElementById("feed-button");
  ok(feedBtn, "Feed button should have been added.");
  let homeBtn = document.getElementById("home-button");
  ok(!homeBtn, "Home button should have been removed.");
  if (feedBtn) {
    let feedParent = feedBtn.parentNode;
    ok(feedParent == navbarCT || feedParent == overflowPanelList,
       "Feed button should be in navbar or overflow");
  }
  navbar.currentSet = currentSet;
  is(currentSet, navbar.currentSet, "Should be able to return to original state.");
});

add_task(function* asyncCleanup() {
  yield resetCustomization();
});
