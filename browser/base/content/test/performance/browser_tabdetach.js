"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * list should slowly go away as we improve the performance of the front-end.
 * Instead of adding more reflows to the list, you should be modifying your code
 * to avoid the reflow.
 *
 * See https://firefox-source-docs.mozilla.org/performance/bestpractices.html
 * for tips on how to do that.
 */
const EXPECTED_REFLOWS = [
  {
    stack: [
      "clientX@chrome://browser/content/tabbrowser-tabs.js",
      "startTabDrag@chrome://browser/content/tabbrowser-tabs.js",
      "on_dragstart@chrome://browser/content/tabbrowser-tabs.js",
      "handleEvent@chrome://browser/content/tabbrowser-tabs.js",
      "synthesizeMouseAtPoint@chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
      "synthesizeMouse@chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
      "synthesizePlainDragAndDrop@chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
    ],
    maxCount: 2,
  },

  {
    stack: [
      "startTabDrag@chrome://browser/content/tabbrowser-tabs.js",
      "on_dragstart@chrome://browser/content/tabbrowser-tabs.js",
      "handleEvent@chrome://browser/content/tabbrowser-tabs.js",
      "synthesizeMouseAtPoint@chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
      "synthesizeMouse@chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
      "synthesizePlainDragAndDrop@chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
    ],
  },
];

/**
 * This test ensures that there are no unexpected uninterruptible reflows when
 * detaching a tab via drag and drop. The first testcase tests a non-overflowed
 * tab strip, and the second tests an overflowed one.
 */

add_task(async function test_detach_not_overflowed() {
  await ensureNoPreloadedBrowser();
  await createTabs(1);

  // Make sure we didn't overflow, as expected
  await TestUtils.waitForCondition(() => {
    return !gBrowser.tabContainer.hasAttribute("overflow");
  });

  let win;
  await withPerfObserver(
    async function() {
      win = await detachTab(gBrowser.tabs[1]);
    },
    {
      expectedReflows: EXPECTED_REFLOWS,
      // we are opening a whole new window, so there's no point in tracking
      // rects being painted
      frames: { filter: rects => [] },
    }
  );

  await BrowserTestUtils.closeWindow(win);
  win = null;
});

add_task(async function test_detach_overflowed() {
  const TAB_COUNT_FOR_OVERFLOW = computeMaxTabCount();
  await createTabs(TAB_COUNT_FOR_OVERFLOW + 1);

  // Make sure we overflowed, as expected
  await TestUtils.waitForCondition(() => {
    return gBrowser.tabContainer.hasAttribute("overflow");
  });

  let win;
  await withPerfObserver(
    async function() {
      win = await detachTab(
        gBrowser.tabs[Math.floor(TAB_COUNT_FOR_OVERFLOW / 2)]
      );
    },
    {
      expectedReflows: EXPECTED_REFLOWS,
      // we are opening a whole new window, so there's no point in tracking
      // rects being painted
      frames: { filter: rects => [] },
    }
  );

  await BrowserTestUtils.closeWindow(win);
  win = null;

  await removeAllButFirstTab();
});

async function detachTab(tab) {
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();

  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab,

    // destElement is null because tab detaching happens due
    // to a drag'n'drop on an invalid drop target.
    destElement: null,

    // don't move horizontally because that could cause a tab move
    // animation, and there's code to prevent a tab detaching if
    // the dragged tab is released while the animation is running.
    stepX: 0,
    stepY: 100,
  });

  return newWindowPromise;
}
