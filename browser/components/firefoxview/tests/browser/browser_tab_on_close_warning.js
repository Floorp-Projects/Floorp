/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

class DialogObserver {
  constructor() {
    this.wasOpened = false;
    Services.obs.addObserver(this, "common-dialog-loaded");
  }
  cleanup() {
    Services.obs.removeObserver(this, "common-dialog-loaded");
  }
  observe(win, topic) {
    if (topic == "common-dialog-loaded") {
      this.wasOpened = true;
      // Close dialog.
      win.document.querySelector("dialog").getButton("cancel").click();
    }
  }
}

add_task(
  async function on_close_warning_should_not_show_for_firefox_view_tab() {
    const dialogObserver = new DialogObserver();
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.warnOnClose", true]],
    });
    info("Opening window...");
    const win = await BrowserTestUtils.openNewBrowserWindow();
    info("Opening Firefox View tab...");
    await openFirefoxViewTab(win);
    info("Trigger warnAboutClosingWindow()");
    win.BrowserCommands.tryToCloseWindow();
    await BrowserTestUtils.closeWindow(win);
    ok(!dialogObserver.wasOpened, "Dialog was not opened");
    dialogObserver.cleanup();
  }
);

add_task(
  async function on_close_warning_should_not_show_for_firefox_view_tab_non_macos() {
    let initialTab = gBrowser.selectedTab;
    const dialogObserver = new DialogObserver();
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.tabs.warnOnClose", true],
        ["browser.warnOnQuit", true],
      ],
    });
    info("Opening Firefox View tab...");
    await openFirefoxViewTab(window);
    info('Trigger "quit-application-requested"');
    canQuitApplication("lastwindow", "close-button");
    ok(!dialogObserver.wasOpened, "Dialog was not opened");
    await BrowserTestUtils.switchTab(gBrowser, initialTab);
    closeFirefoxViewTab(window);
    dialogObserver.cleanup();
  }
);
