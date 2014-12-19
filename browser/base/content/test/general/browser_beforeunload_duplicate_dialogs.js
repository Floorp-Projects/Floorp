const TEST_PAGE = "http://mochi.test:8888/browser/browser/base/content/test/general/file_double_close_tab.html";

let expectingDialog = false;
function onTabModalDialogLoaded(node) {
  ok(expectingDialog, "Should be expecting this dialog.");
  expectingDialog = false;
  // This accepts the dialog, closing it
  node.Dialog.ui.button0.click();
}


// Listen for the dialog being created
Services.obs.addObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded", false);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.tabs.warnOnClose");
  Services.obs.removeObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
});

add_task(function* closeLastTabInWindow() {
  let newWin = yield promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  yield promiseTabLoadEvent(firstTab, TEST_PAGE);
  let windowClosedPromise = promiseWindowClosed(newWin);
  expectingDialog = true;
  // close tab:
  document.getAnonymousElementByAttribute(firstTab, "anonid", "close-button").click();
  yield windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});

add_task(function* closeWindowWithMultipleTabsIncludingOneBeforeUnload() {
  Services.prefs.setBoolPref("browser.tabs.warnOnClose", false);
  let newWin = yield promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  yield promiseTabLoadEvent(firstTab, TEST_PAGE);
  yield promiseTabLoadEvent(newWin.gBrowser.addTab(), "http://example.com/");
  let windowClosedPromise = promiseWindowClosed(newWin);
  expectingDialog = true;
  newWin.BrowserTryToCloseWindow();
  yield windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});
