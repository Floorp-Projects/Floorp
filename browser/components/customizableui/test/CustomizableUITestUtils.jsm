/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Shared functions generally available for tests involving PanelMultiView and
 * the CustomizableUI elements in the browser window.
 */

var EXPORTED_SYMBOLS = ["CustomizableUITestUtils"];

ChromeUtils.import("resource://testing-common/Assert.jsm");
ChromeUtils.import("resource://testing-common/BrowserTestUtils.jsm");
ChromeUtils.import("resource://testing-common/TestUtils.jsm");

ChromeUtils.defineModuleGetter(this, "CustomizableUI",
                               "resource:///modules/CustomizableUI.jsm");

class CustomizableUITestUtils {
  /**
   * Constructs an instance that operates with the specified browser window.
   */
  constructor(window) {
    this.window = window;
    this.document = window.document;
    this.PanelUI = window.PanelUI;
  }

  /**
   * Opens a closed PanelMultiView via the specified function while waiting for
   * the main view with the specified ID to become fully interactive.
   */
  async openPanelMultiView(panel, mainView, openFn) {
    if (panel.state == "open") {
      // Some tests may intermittently leave the panel open. We report this, but
      // don't fail so we don't introduce new intermittent test failures.
      Assert.ok(true, "A previous test left the panel open. This should be" +
                      " fixed, but we can still do a best-effort recovery and" +
                      " assume that the requested view will be made visible.");
      await openFn();
      return;
    }

    if (panel.state == "hiding") {
      // There may still be tests that don't wait after invoking a command that
      // causes the main menu panel to close. Depending on timing, the panel may
      // or may not be fully closed when the following test runs. We handle this
      // case gracefully so we don't risk introducing new intermittent test
      // failures that may show up at a later time.
      Assert.ok(true, "A previous test requested the panel to close but" +
                      " didn't wait for the operation to complete. While" +
                      " the test should be fixed, we can still continue.");
    } else {
      Assert.equal(panel.state, "closed", "The panel is closed to begin with.");
    }

    let promiseShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
    await openFn();
    await promiseShown;
  }

  /**
   * Closes an open PanelMultiView via the specified function while waiting for
   * the operation to complete.
   */
  async hidePanelMultiView(panel, closeFn) {
    Assert.ok(panel.state == "open", "The panel is open to begin with.");

    let promiseHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
    await closeFn();
    await promiseHidden;
  }

  /**
   * Opens the main menu and waits for it to become fully interactive.
   */
  async openMainMenu() {
    await this.openPanelMultiView(this.PanelUI.panel, this.PanelUI.mainView,
                                  () => this.PanelUI.show());
  }

  /**
   * Closes the main menu and waits for the operation to complete.
   */
  async hideMainMenu() {
    await this.hidePanelMultiView(this.PanelUI.panel,
                                  () => this.PanelUI.hide());
  }

  /**
   * Add the search bar into the nav bar and verify it does not overflow.
   *
   * @returns {Promise}
   * @resolves The search bar element.
   * @rejects If search bar is not found, or overflows.
   */
  async addSearchBar() {
    CustomizableUI.addWidgetToArea(
      "search-container", CustomizableUI.AREA_NAVBAR,
      CustomizableUI.getPlacementOfWidget("urlbar-container").position + 1);

    // addWidgetToArea adds the search bar into the nav bar first.  If the
    // search bar overflows, OverflowableToolbar for the nav bar moves the
    // search bar into the overflow panel in its overflow event handler
    // asynchronously.
    //
    // We should first wait for the layout flush to make sure either the search
    // bar fits into the nav bar, or overflow event gets dispatched and the
    // overflow event handler is called.
    await this.window.promiseDocumentFlushed(() => {});

    // Check if the OverflowableToolbar is handling the overflow event.
    // _lastOverflowCounter property is incremented synchronously at the top
    // of the overflow event handler, and is set to 0 when it finishes.
    let navbar = this.window.document.getElementById(CustomizableUI.AREA_NAVBAR);
    await TestUtils.waitForCondition(() => {
      // This test is using a private variable, that can be renamed or removed
      // in the future.  Use === so that this won't silently skip if the value
      // becomes undefined.
      return navbar.overflowable._lastOverflowCounter === 0;
    });

    let searchbar = this.window.document.getElementById("searchbar");
    if (!searchbar) {
      throw new Error("The search bar should exist.");
    }

    // If the search bar overflows, it's placed inside the overflow panel.
    //
    // We cannot use navbar's property to check if overflow happens, since it
    // can be different widget than the search bar that overflows.
    if (searchbar.closest("#widget-overflow")) {
      throw new Error("The search bar should not overflow from the nav bar. " +
                      "This test fails if the screen resolution is small and " +
                      "the search bar overflows from the nav bar.");
    }

    return searchbar;
  }

  removeSearchBar() {
    CustomizableUI.removeWidgetFromArea("search-container");
  }
}
