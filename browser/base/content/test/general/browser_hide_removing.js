/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Bug 587922: tabs don't get removed if they're hidden

add_task(async function() {
  // Add a tab that will get removed and hidden
  let testTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  is(gBrowser.visibleTabs.length, 2, "just added a tab, so 2 tabs");
  await BrowserTestUtils.switchTab(gBrowser, testTab);

  let numVisBeforeHide, numVisAfterHide;

  // We have to animate the tab removal in order to get an async
  // tab close.
  BrowserTestUtils.removeTab(testTab, { animate: true });

  numVisBeforeHide = gBrowser.visibleTabs.length;
  gBrowser.hideTab(testTab);
  numVisAfterHide = gBrowser.visibleTabs.length;

  is(numVisBeforeHide, 1, "animated remove has in 1 tab left");
  is(numVisAfterHide, 1, "hiding a removing tab also has 1 tab");
});
