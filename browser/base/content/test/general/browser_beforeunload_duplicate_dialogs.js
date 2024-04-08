const TEST_PAGE =
  "http://mochi.test:8888/browser/browser/base/content/test/general/file_double_close_tab.html";

var expectingDialog = false;
var wantToClose = true;
var resolveDialogPromise;

function onCommonDialogLoaded(promptWindow) {
  ok(expectingDialog, "Should be expecting this dialog.");
  expectingDialog = false;
  let dialog = promptWindow.Dialog;
  if (wantToClose) {
    // This accepts the dialog, closing it.
    dialog.ui.button0.click();
  } else {
    // This keeps the page open
    dialog.ui.button1.click();
  }
  if (resolveDialogPromise) {
    resolveDialogPromise();
  }
}

SpecialPowers.pushPrefEnv({
  set: [["dom.require_user_interaction_for_beforeunload", false]],
});

// Listen for the dialog being created
Services.obs.addObserver(onCommonDialogLoaded, "common-dialog-loaded");
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.tabs.warnOnClose");
  Services.obs.removeObserver(onCommonDialogLoaded, "common-dialog-loaded");
});

add_task(async function closeLastTabInWindow() {
  let newWin = await promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  await promiseTabLoadEvent(firstTab, TEST_PAGE);
  let windowClosedPromise = BrowserTestUtils.domWindowClosed(newWin);
  expectingDialog = true;
  // close tab:
  firstTab.closeButton.click();
  await windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});

add_task(async function closeWindowWithMultipleTabsIncludingOneBeforeUnload() {
  Services.prefs.setBoolPref("browser.tabs.warnOnClose", false);
  let newWin = await promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  await promiseTabLoadEvent(firstTab, TEST_PAGE);
  await promiseTabLoadEvent(
    BrowserTestUtils.addTab(newWin.gBrowser),
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/"
  );
  let windowClosedPromise = BrowserTestUtils.domWindowClosed(newWin);
  expectingDialog = true;
  newWin.BrowserCommands.tryToCloseWindow();
  await windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
  Services.prefs.clearUserPref("browser.tabs.warnOnClose");
});

add_task(async function closeWindoWithSingleTabTwice() {
  let newWin = await promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  await promiseTabLoadEvent(firstTab, TEST_PAGE);
  let windowClosedPromise = BrowserTestUtils.domWindowClosed(newWin);
  expectingDialog = true;
  wantToClose = false;
  let firstDialogShownPromise = new Promise(resolve => {
    resolveDialogPromise = resolve;
  });
  firstTab.closeButton.click();
  await firstDialogShownPromise;
  info("Got initial dialog, now trying again");
  expectingDialog = true;
  wantToClose = true;
  resolveDialogPromise = null;
  firstTab.closeButton.click();
  await windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});
