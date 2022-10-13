/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  async function on_close_warning_should_not_show_for_firefox_view_tab() {
    let dialogOpened = false;
    function setDialogOpened() {
      dialogOpened = true;
    }
    Services.obs.addObserver(setDialogOpened, "common-dialog-loaded");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.warnOnClose", true]],
    });
    info("Opening window...");
    const win = await BrowserTestUtils.openNewBrowserWindow({
      waitForTabURL: "about:newtab",
    });
    info("Opening Firefox View tab...");
    await openFirefoxViewTab(win);
    // Trigger warnAboutClosingWindow()
    win.BrowserTryToCloseWindow();
    await BrowserTestUtils.closeWindow(win);
    ok(!dialogOpened, "Dialog was not opened");
    Services.obs.removeObserver(setDialogOpened, "common-dialog-loaded");
  }
);
