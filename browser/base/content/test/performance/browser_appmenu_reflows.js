"use strict";

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
  [
    "openPopup@chrome://global/content/bindings/popup.xml",
    "show/</<@chrome://browser/content/customizableui/panelUI.js",
  ],

  [
    "get_alignmentPosition@chrome://global/content/bindings/popup.xml",
    "adjustArrowPosition@chrome://global/content/bindings/popup.xml",
    "onxblpopupshowing@chrome://global/content/bindings/popup.xml",
    "openPopup@chrome://global/content/bindings/popup.xml",
    "show/</<@chrome://browser/content/customizableui/panelUI.js",
  ],

  [
    "get_alignmentPosition@chrome://global/content/bindings/popup.xml",
    "adjustArrowPosition@chrome://global/content/bindings/popup.xml",
    "onxblpopupshowing@chrome://global/content/bindings/popup.xml",
    "openPopup@chrome://global/content/bindings/popup.xml",
    "show/</<@chrome://browser/content/customizableui/panelUI.js",
  ],

  [
    "get_alignmentPosition@chrome://global/content/bindings/popup.xml",
    "adjustArrowPosition@chrome://global/content/bindings/popup.xml",
    "onxblpopuppositioned@chrome://global/content/bindings/popup.xml",
  ],

  [
    "get_alignmentPosition@chrome://global/content/bindings/popup.xml",
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],

  [
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],

  [
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],

  [
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],

  [
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],

  [
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],

  [
    "handleEvent@resource:///modules/PanelMultiView.jsm",
    "openPopup@chrome://global/content/bindings/popup.xml",
  ],
];

const EXPECTED_APPMENU_SUBVIEW_REFLOWS = [
  /**
   * The synced tabs view has labels that are multiline. Because of bugs in
   * XUL layout relating to multiline text in scrollable containers, we need
   * to manually read their height in order to ensure container heights are
   * correct. Unfortunately this requires 2 sync reflows.
   *
   * If we add more views where this is necessary, we may need to duplicate
   * these expected reflows further.
   *
   * Because the test dirties the frame tree by manipulating margins,
   * getBoundingClientRect() in the descriptionHeightWorkaround code
   * seems to sometimes fire multiple times. Bug 1363361 will change how the
   * test dirties the frametree, after which this (2 hits in that method)
   * should become deterministic and we can re-enable the subview testing
   * for the remotetabs subview (this is bug 1376822). In the meantime,
   * that subview only is excluded from this test.
  [
    "descriptionHeightWorkaround@resource:///modules/PanelMultiView.jsm",
    "onTransitionEnd@resource:///modules/PanelMultiView.jsm",
  ],
  [
    "descriptionHeightWorkaround@resource:///modules/PanelMultiView.jsm",
    "onTransitionEnd@resource:///modules/PanelMultiView.jsm",
  ],
   */
  /**
   * Please don't add anything new!
   */
];

add_task(async function() {
  await ensureNoPreloadedBrowser();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.photon.structure.enabled", true]],
  });

  // First, open the appmenu.
  await withReflowObserver(async function() {
    let popupPositioned =
      BrowserTestUtils.waitForEvent(PanelUI.panel, "popuppositioned");
    await PanelUI.show();
    await popupPositioned;
  }, EXPECTED_APPMENU_OPEN_REFLOWS);

  // Now open a series of subviews, and then close the appmenu. We
  // should not reflow during any of this.
  await withReflowObserver(async function() {
    // This recursive function will take the current main or subview,
    // find all of the buttons that navigate to subviews inside it,
    // and click each one individually. Upon entering the new view,
    // we recurse. When the subviews within a view have been
    // exhausted, we go back up a level.
    async function openSubViewsRecursively(currentView) {
      let navButtons = Array.from(currentView.querySelectorAll(".subviewbutton-nav"));
      if (!navButtons) {
        return;
      }

      for (let button of navButtons) {
        // We skip the remote tabs subview, see the comments above
        // in EXPECTED_APPMENU_SUBVIEW_REFLOWS. bug 1376822 tracks
        // re-enabling this.
        if (button.id == "appMenu-library-remotetabs-button") {
          info("Skipping " + button.id);
          continue;
        }
        info("Click " + button.id);
        button.click();
        await BrowserTestUtils.waitForEvent(PanelUI.panel, "ViewShown");
        info("Shown " + PanelUI.multiView.instance._currentSubView.id);
        // Unfortunately, I can't find a better accessor to the current
        // subview, so I have to reach the PanelMultiView instance
        // here.
        await openSubViewsRecursively(PanelUI.multiView.instance._currentSubView);
        PanelUI.multiView.goBack();
        await BrowserTestUtils.waitForEvent(PanelUI.panel, "ViewShown");
      }
    }

    await openSubViewsRecursively(PanelUI.mainView);

    let hidden = BrowserTestUtils.waitForEvent(PanelUI.panel, "popuphidden");
    PanelUI.hide();
    await hidden;
  }, EXPECTED_APPMENU_SUBVIEW_REFLOWS);
});
