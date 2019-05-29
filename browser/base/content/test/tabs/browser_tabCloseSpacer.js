/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that while clicking to close tabs, the close button remains under the mouse
 * even when an underflow happens.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["toolkit.cosmeticAnimations.enabled", false],
    ],
  });

  let downButton = gBrowser.tabContainer.arrowScrollbox._scrollButtonDown;
  let closingTabsSpacer = gBrowser.tabContainer._closingTabsSpacer;

  await overflowTabs();
  ok(gBrowser.tabContainer.hasAttribute("overflow"), "Tab strip should be overflowing");
  isnot(downButton.clientWidth, 0, "down button has some width");
  is(closingTabsSpacer.clientWidth, 0, "spacer has no width");

  let originalCloseButtonLocation = getLastCloseButtonLocation();

  info("Removing half the tabs and making sure the last close button doesn't move");
  let numTabs = gBrowser.tabs.length / 2;
  while (gBrowser.tabs.length > numTabs) {
    let lastCloseButtonLocation = getLastCloseButtonLocation();
    Assert.deepEqual(lastCloseButtonLocation, originalCloseButtonLocation, "Close button hasn't moved");

    EventUtils.synthesizeMouseAtCenter(getLastCloseButton(), {});
    await new Promise(r => requestAnimationFrame(r));
  }

  ok(!gBrowser.tabContainer.hasAttribute("overflow"), "not overflowing");
  ok(gBrowser.tabContainer.hasAttribute("using-closing-tabs-spacer"), "using spacer");
  is(downButton.clientWidth, 0, "down button has no width");
  isnot(closingTabsSpacer.clientWidth, 0, "spacer has some width");
});

async function overflowTabs() {
  let arrowScrollbox = gBrowser.tabContainer.arrowScrollbox;
  let width = ele => ele.getBoundingClientRect().width;
  let tabMinWidth = parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth);
  let tabCountForOverflow = Math.ceil(width(arrowScrollbox) / tabMinWidth * 1.1);
  while (gBrowser.tabs.length < tabCountForOverflow) {
    BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true, index: 0 });
  }
  await window.promiseDocumentFlushed(() => {});
}

function getLastCloseButton() {
  let lastTab = gBrowser.tabs[gBrowser.tabs.length - 1];
  return document.getAnonymousElementByAttribute(lastTab, "anonid", "close-button");
}

function getLastCloseButtonLocation() {
  let rect = getLastCloseButton().getBoundingClientRect();
  return {
    left: Math.round(rect.left),
    top: Math.round(rect.top),
    width: Math.round(rect.width),
    height: Math.round(rect.height),
  };
}

registerCleanupFunction(() => {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});
