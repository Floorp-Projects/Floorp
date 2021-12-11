/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TARGET_PANES = [
  {
    title: "Temporary Extensions",
    pref: "devtools.aboutdebugging.collapsibilities.temporaryExtension",
  },
  {
    title: "Extensions",
    pref: "devtools.aboutdebugging.collapsibilities.installedExtension",
  },
  {
    title: "Tabs",
    pref: "devtools.aboutdebugging.collapsibilities.tab",
  },
  {
    title: "Service Workers",
    pref: "devtools.aboutdebugging.collapsibilities.serviceWorker",
  },
  {
    title: "Shared Workers",
    pref: "devtools.aboutdebugging.collapsibilities.sharedWorker",
  },
  {
    title: "Other Workers",
    pref: "devtools.aboutdebugging.collapsibilities.otherWorker",
  },
];
/* exported TARGET_PANES */

function prepareCollapsibilitiesTest() {
  // Make all collapsibilities to be expanded.
  for (const { pref } of TARGET_PANES) {
    Services.prefs.setBoolPref(pref, false);
  }
}
/* exported prepareCollapsibilitiesTest */

async function toggleCollapsibility(debugTargetPane) {
  debugTargetPane.querySelector(".qa-debug-target-pane-title").click();
  // Wait for animation of collapse/expand.
  const animations = debugTargetPane.ownerDocument.getAnimations();
  await Promise.all(animations.map(animation => animation.finished));
}
/* exported toggleCollapsibility */

registerCleanupFunction(() => {
  for (const { pref } of TARGET_PANES) {
    Services.prefs.clearUserPref(pref);
  }
});
