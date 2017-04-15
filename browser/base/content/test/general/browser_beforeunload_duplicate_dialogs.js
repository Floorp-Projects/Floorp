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

add_task(function* closeLastTabInWindow() {
  let newWin = yield promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  yield promiseTabLoadEvent(firstTab, TEST_PAGE);
  let windowClosedPromise = promiseWindowWillBeClosed(newWin);
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
  let windowClosedPromise = promiseWindowWillBeClosed(newWin);
  expectingDialog = true;
  newWin.BrowserTryToCloseWindow();
  yield windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
  Services.prefs.clearUserPref("browser.tabs.warnOnClose");
});

add_task(function* closeWindoWithSingleTabTwice() {
  let newWin = yield promiseOpenAndLoadWindow({}, true);
  let firstTab = newWin.gBrowser.selectedTab;
  yield promiseTabLoadEvent(firstTab, TEST_PAGE);
  let windowClosedPromise = promiseWindowWillBeClosed(newWin);
  expectingDialog = true;
  wantToClose = false;
  let firstDialogShownPromise = new Promise((resolve, reject) => { resolveDialogPromise = resolve; });
  document.getAnonymousElementByAttribute(firstTab, "anonid", "close-button").click();
  yield firstDialogShownPromise;
  info("Got initial dialog, now trying again");
  expectingDialog = true;
  wantToClose = true;
  resolveDialogPromise = null;
  document.getAnonymousElementByAttribute(firstTab, "anonid", "close-button").click();
  yield windowClosedPromise;
  ok(!expectingDialog, "There should have been a dialog.");
  ok(newWin.closed, "Window should be closed.");
});
