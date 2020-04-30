/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new tabs.
 */
add_task(async function() {
  // Force-enable tab animations
  gReduceMotionOverride = false;

  await ensureNoPreloadedBrowser();
  await disableFxaBadge();

  // Prepare the window to avoid flicker and reflow that's unrelated to our
  // tab opening operation.
  gURLBar.focus();

  let tabStripRect = gBrowser.tabContainer.arrowScrollbox.getBoundingClientRect();
  let firstTabRect = gBrowser.selectedTab.getBoundingClientRect();
  let firstTabLabelRect = gBrowser.selectedTab.textLabel.getBoundingClientRect();
  let textBoxRect = gURLBar
    .querySelector("moz-input-box")
    .getBoundingClientRect();

  let inRange = (val, min, max) => min <= val && val <= max;

  // Add a reflow observer and open a new tab.
  await withPerfObserver(
    async function() {
      let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
      BrowserOpenTab();
      await BrowserTestUtils.waitForEvent(
        gBrowser.selectedTab,
        "TabAnimationEnd"
      );
      await switchDone;
    },
    {
      expectedReflows: EXPECTED_REFLOWS,
      frames: {
        filter: rects =>
          rects.filter(
            r =>
              !(
                // We expect all changes to be within the tab strip.
                (
                  r.y1 >= tabStripRect.top &&
                  r.y2 <= tabStripRect.bottom &&
                  r.x1 >= tabStripRect.left &&
                  r.x2 <= tabStripRect.right &&
                  // The first tab should get deselected at the same time as the next
                  // tab starts appearing, so we should have one rect that includes the
                  // first tab but is wider.
                  ((inRange(r.w, firstTabRect.width, firstTabRect.width * 2) &&
                    r.x1 == firstTabRect.x) ||
                  // The second tab gets painted several times due to tabopen animation.
                  (inRange(
                    r.x1,
                    firstTabRect.right - 1, // -1 for the border on Win7
                    firstTabRect.right + firstTabRect.width
                  ) &&
                    r.x2 < firstTabRect.right + firstTabRect.width + 25) || // The + 25 is because sometimes the '+' is in the same rect.
                    // The '+' icon moves with an animation. At the end of the animation
                    // the former and new positions can touch each other causing the rect
                    // to have twice the icon's width.
                    (r.h == 14 && r.w <= 2 * 14 + kMaxEmptyPixels) ||
                    // We sometimes have a rect for the right most 2px of the '+' button.
                    (r.h == 2 && r.w == 2) ||
                    // Same for the 'X' icon.
                    (r.h == 10 && r.w <= 2 * 10))
                )
              )
          ),
        exceptions: [
          {
            name:
              "bug 1446452 - the new tab should appear at the same time as the" +
              " previous one gets deselected",
            condition: r =>
              // In tab strip
              r.y1 >= tabStripRect.top &&
              r.y2 <= tabStripRect.bottom &&
              // Position and size of the first tab.
              r.x1 == firstTabRect.left &&
              inRange(
                r.w,
                firstTabRect.width - 1, // -1 as the border doesn't change
                firstTabRect.width
              ),
          },
          {
            name: "the urlbar placeolder moves up and down by a few pixels",
            // This seems to only happen on the second run in --verify
            condition: r =>
              r.x1 >= textBoxRect.left &&
              r.x2 <= textBoxRect.right &&
              r.y1 >= textBoxRect.top &&
              r.y2 <= textBoxRect.bottom,
          },
          {
            name:
              "bug 1477966 - the name of a deselected tab should appear immediately",
            condition: r =>
              AppConstants.platform == "macosx" &&
              r.x1 >= firstTabLabelRect.x &&
              r.x2 <= firstTabLabelRect.right &&
              r.y1 >= firstTabLabelRect.y &&
              r.y2 <= firstTabLabelRect.bottom,
          },
        ],
      },
    }
  );

  let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await switchDone;
});
