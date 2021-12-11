/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that when different combinations of warnings are enabled,
 * quitting produces the correct warning (if any), and the checkbox
 * is also correct.
 */
add_task(async function test_check_right_prompt() {
  let tests = [
    {
      warnOnQuitShortcut: true,
      warnOnClose: false,
      expectedDialog: "shortcut",
      messageSuffix: "with shortcut but no tabs warning",
    },
    {
      warnOnQuitShortcut: false,
      warnOnClose: true,
      expectedDialog: "tabs",
      messageSuffix: "with tabs but no shortcut warning",
    },
    {
      warnOnQuitShortcut: false,
      warnOnClose: false,
      messageSuffix: "with no warning",
      expectedDialog: null,
    },
    {
      warnOnQuitShortcut: true,
      warnOnClose: true,
      messageSuffix: "with both warnings",
      // Note: this is somewhat arbitrary; I don't think there's a right/wrong
      // here, so if this changes due to implementation details, updating the
      // text expectation to be "tabs" should be OK.
      expectedDialog: "shortcut",
    },
  ];
  let tab = BrowserTestUtils.addTab(gBrowser);

  function checkDialog(dialog, expectedDialog, messageSuffix) {
    let dialogElement = dialog.document.getElementById("commonDialog");
    let acceptLabel = dialogElement.getButton("accept").label;
    is(
      acceptLabel.startsWith("Quit"),
      expectedDialog == "shortcut",
      `dialog label ${
        expectedDialog == "shortcut" ? "should" : "should not"
      } start with Quit ${messageSuffix}`
    );
    let checkLabel = dialogElement.querySelector("checkbox").label;
    is(
      checkLabel.includes("before quitting with"),
      expectedDialog == "shortcut",
      `checkbox label ${
        expectedDialog == "shortcut" ? "should" : "should not"
      } be for quitting ${messageSuffix}`
    );

    dialogElement.getButton("cancel").click();
  }

  let dialogOpened = false;
  function setDialogOpened() {
    dialogOpened = true;
  }
  Services.obs.addObserver(setDialogOpened, "common-dialog-loaded");
  for (let {
    warnOnClose,
    warnOnQuitShortcut,
    expectedDialog,
    messageSuffix,
  } of tests) {
    dialogOpened = false;
    let promise = null;
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.tabs.warnOnClose", warnOnClose],
        ["browser.warnOnQuitShortcut", warnOnQuitShortcut],
        ["browser.warnOnQuit", true],
      ],
    });
    if (expectedDialog) {
      promise = BrowserTestUtils.promiseAlertDialogOpen("", undefined, {
        callback(win) {
          checkDialog(win, expectedDialog, messageSuffix);
        },
      });
    }
    is(
      !canQuitApplication(undefined, "shortcut"),
      !!expectedDialog,
      `canQuitApplication ${
        expectedDialog ? "should" : "should not"
      } block ${messageSuffix}.`
    );
    await promise;
    is(
      dialogOpened,
      !!expectedDialog,
      `Should ${
        expectedDialog ? "" : "not "
      }have opened a dialog ${messageSuffix}.`
    );
  }
  Services.obs.removeObserver(setDialogOpened, "common-dialog-loaded");
  BrowserTestUtils.removeTab(tab);
});
