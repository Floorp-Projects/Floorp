const TEST_PAGE = "http://mochi.test:8888/browser/browser/base/content/test/general/file_double_close_tab.html";

var expectingDialog = false;
var wantToClose = true;
var resolveDialogPromise;
function onTabModalDialogLoaded(node) {
  ok(expectingDialog, "Should be expecting this dialog.");
  expectingDialog = false;
  if (wantToClose) {
    // This accepts the dialog, closing it
    node.Dialog.ui.button0.click();
  } else {
    // This keeps the page open
    node.Dialog.ui.button1.click();
  }
  if (resolveDialogPromise) {
    resolveDialogPromise();
  }
}

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

// Listen for the dialog being created
Services.obs.addObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.tabs.warnOnClose");
  Services.obs.removeObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
});

add_task(async function closeLastTabInWindow() {
  let newWin = await promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  await promiseTabLoadEvent(firstTab, TEST_PAGE);
  let windowClosedPromise = BrowserTestUtils.domWindowClosed(newWin);
  expectingDialog = true;
  // close tab:
  document.getAnonymousElementByAttribute(firstTab, "anonid", "close-button").click();
  await windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});

add_task(async function closeWindowWithMultipleTabsIncludingOneBeforeUnload() {
  Services.prefs.setBoolPref("browser.tabs.warnOnClose", false);
  let newWin = await promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  await promiseTabLoadEvent(firstTab, TEST_PAGE);
  await promiseTabLoadEvent(newWin.gBrowser.addTab(), "http://example.com/");
  let windowClosedPromise = BrowserTestUtils.domWindowClosed(newWin);
  expectingDialog = true;
  newWin.BrowserTryToCloseWindow();
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
  let firstDialogShownPromise = new Promise((resolve, reject) => { resolveDialogPromise = resolve; });
  document.getAnonymousElementByAttribute(firstTab, "anonid", "close-button").click();
  await firstDialogShownPromise;
  info("Got initial dialog, now trying again");
  expectingDialog = true;
  wantToClose = true;
  resolveDialogPromise = null;
  document.getAnonymousElementByAttribute(firstTab, "anonid", "close-button").click();
  await windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});
