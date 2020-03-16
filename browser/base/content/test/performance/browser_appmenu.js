"use strict";
/* global PanelUI */

ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm",
  this
);
let gCUITestUtils = new CustomizableUITestUtils(window);

/**
 * WHOA THERE: We should never be adding new things to
 * EXPECTED_APPMENU_OPEN_REFLOWS. This is a whitelist that should slowly go
 * away as we improve the performance of the front-end. Instead of adding more
 * reflows to the whitelist, you should be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_APPMENU_OPEN_REFLOWS = [
  {
    stack: [
      "openPopup/this._openPopupPromise<@resource:///modules/PanelMultiView.jsm",
    ],
  },

  {
    stack: [
      "adjustArrowPosition@chrome://global/content/elements/panel.js",
      "on_popuppositioned@chrome://global/content/elements/panel.js",
    ],

    maxCount: 22, // This number should only ever go down - never up.
  },

  {
    stack: [
      "_calculateMaxHeight@resource:///modules/PanelMultiView.jsm",
      "handleEvent@resource:///modules/PanelMultiView.jsm",
    ],

    maxCount: 7, // This number should only ever go down - never up.
  },
];

add_task(async function() {
  await ensureNoPreloadedBrowser();

  let textBoxRect = gURLBar
    .querySelector("moz-input-box")
    .getBoundingClientRect();
  let menuButtonRect = document
    .getElementById("PanelUI-menu-button")
    .getBoundingClientRect();
  let fxaToolbarButtonRect = document
    .getElementById("fxa-toolbar-menu-button")
    .getBoundingClientRect();
  let firstTabRect = gBrowser.selectedTab.getBoundingClientRect();
  let frameExpectations = {
    filter: rects =>
      rects.filter(
        r =>
          !(
            // We expect the menu button to get into the active state.
            (
              r.y1 >= menuButtonRect.top &&
              r.y2 <= menuButtonRect.bottom &&
              r.x1 >= menuButtonRect.left &&
              r.x2 <= menuButtonRect.right
            )
          )
        // XXX For some reason the menu panel isn't in our screenshots,
        // but that's where we actually expect many changes.
      ),
    exceptions: [
      {
        name: "the urlbar placeolder moves up and down by a few pixels",
        condition: r =>
          r.x1 >= textBoxRect.left &&
          r.x2 <= textBoxRect.right &&
          r.y1 >= textBoxRect.top &&
          r.y2 <= textBoxRect.bottom,
      },
      {
        name: "bug 1547341 - a first tab gets drawn early",
        condition: r =>
          r.x1 >= firstTabRect.left &&
          r.x2 <= firstTabRect.right &&
          r.y1 >= firstTabRect.top &&
          r.y2 <= firstTabRect.bottom,
      },
      {
        name: "the fxa toolbar changes icon when first clicked",
        condition: r =>
          r.x1 >= fxaToolbarButtonRect.left &&
          r.x2 <= fxaToolbarButtonRect.right &&
          r.y1 >= fxaToolbarButtonRect.top &&
          r.y2 <= fxaToolbarButtonRect.bottom,
      },
    ],
  };

  // First, open the appmenu.
  await withPerfObserver(() => gCUITestUtils.openMainMenu(), {
    expectedReflows: EXPECTED_APPMENU_OPEN_REFLOWS,
    frames: frameExpectations,
  });

  // Now open a series of subviews, and then close the appmenu. We
  // should not reflow during any of this.
  await withPerfObserver(
    async function() {
      // This recursive function will take the current main or subview,
      // find all of the buttons that navigate to subviews inside it,
      // and click each one individually. Upon entering the new view,
      // we recurse. When the subviews within a view have been
      // exhausted, we go back up a level.
      async function openSubViewsRecursively(currentView) {
        let navButtons = Array.from(
          // Ensure that only enabled buttons are tested
          currentView.querySelectorAll(".subviewbutton-nav:not([disabled])")
        );
        if (!navButtons) {
          return;
        }

        for (let button of navButtons) {
          info("Click " + button.id);
          let promiseViewShown = BrowserTestUtils.waitForEvent(
            PanelUI.panel,
            "ViewShown"
          );
          button.click();
          let viewShownEvent = await promiseViewShown;

          // Workaround until bug 1363756 is fixed, then this can be removed.
          let container = PanelUI.multiView.querySelector(
            ".panel-viewcontainer"
          );
          await TestUtils.waitForCondition(() => {
            return !container.hasAttribute("width");
          });

          info("Shown " + viewShownEvent.originalTarget.id);
          await openSubViewsRecursively(viewShownEvent.originalTarget);
          promiseViewShown = BrowserTestUtils.waitForEvent(
            currentView,
            "ViewShown"
          );
          PanelUI.multiView.goBack();
          await promiseViewShown;

          // Workaround until bug 1363756 is fixed, then this can be removed.
          await TestUtils.waitForCondition(() => {
            return !container.hasAttribute("width");
          });
        }
      }

      await openSubViewsRecursively(PanelUI.mainView);

      await gCUITestUtils.hideMainMenu();
    },
    { expectedReflows: [], frames: frameExpectations }
  );
});
