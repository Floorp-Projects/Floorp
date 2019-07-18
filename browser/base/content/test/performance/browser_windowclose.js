"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_REFLOWS = [
  /**
   * Nothing here! Please don't add anything new!
   */
];

/**
 * This test ensures that there are no unexpected
 * uninterruptible reflows when closing windows. When the
 * window is closed, the test waits until the original window
 * has activated.
 */
add_task(async function() {
  // Ensure that this browser window starts focused. This seems to be
  // necessary to avoid intermittent failures when running this test
  // on repeat.
  await new Promise(resolve => {
    waitForFocus(resolve, window);
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await new Promise(resolve => {
    waitForFocus(resolve, win);
  });

  // At the time of writing, there are no reflows on window closing.
  // Mochitest will fail if we have no assertions, so we add one here
  // to make sure nobody adds any new ones.
  Assert.equal(
    EXPECTED_REFLOWS.length,
    0,
    "We shouldn't have added any new expected reflows for window close."
  );

  let dropmarkerRect = gURLBar.dropmarker.getBoundingClientRect();

  await withPerfObserver(
    async function() {
      let promiseOrigBrowserFocused = BrowserTestUtils.waitForCondition(() => {
        return Services.focus.activeWindow == window;
      });
      await BrowserTestUtils.closeWindow(win);
      await promiseOrigBrowserFocused;
    },
    {
      expectedReflows: EXPECTED_REFLOWS,
      frames: {
        filter: rects =>
          rects.filter(
            r =>
              !// The dropmarker is visible when the window opens and sometimes hasn't
              // finished its transition to opacity: 0 by the time waitForFocus resolves.
              (
                r.x1 >= dropmarkerRect.left - 1 &&
                r.x2 <= dropmarkerRect.right + 1 &&
                r.y1 >= dropmarkerRect.top &&
                r.y2 <= dropmarkerRect.bottom
              )
          ),
      },
    },
    win
  );
});
