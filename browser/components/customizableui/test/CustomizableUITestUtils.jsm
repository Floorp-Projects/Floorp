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
      Assert.ok(panel.state == "closed", "The panel is closed to begin with.");
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
}
