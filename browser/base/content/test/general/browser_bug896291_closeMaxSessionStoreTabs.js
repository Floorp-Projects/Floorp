function numClosedTabs()
  Cc["@mozilla.org/browser/sessionstore;1"].
    getService(Ci.nsISessionStore).
    getNumberOfTabsClosedLast(window);

let originalTab;
let maxTabsUndo;
let maxTabsUndoPlusOne;
let acceptRemoveAllTabsDialogListener;
let cancelRemoveAllTabsDialogListener;

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref("browser.tabs.animate", false);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.tabs.animate");

    originalTab.linkedBrowser.loadURI("about:blank");
    originalTab = null;
  });

  // Creating and throwing away this tab guarantees that the
  // number of tabs closed in the previous tab-close operation is 1.
  let throwaway_tab = gBrowser.addTab("http://mochi.test:8888/");
  gBrowser.removeTab(throwaway_tab);

  let undoCloseTabElement = document.getElementById("context_undoCloseTab");
  updateTabContextMenu();
  is(undoCloseTabElement.label, undoCloseTabElement.getAttribute("singletablabel"),
     "The label should be showing that the command will restore a single tab");

  originalTab = gBrowser.selectedTab;
  gBrowser.selectedBrowser.loadURI("http://mochi.test:8888/");

  maxTabsUndo = Services.prefs.getIntPref("browser.sessionstore.max_tabs_undo");
  maxTabsUndoPlusOne = maxTabsUndo + 1;
  let numberOfTabsLoaded = 0;
  for (let i = 0; i < maxTabsUndoPlusOne; i++) {
    let tab = gBrowser.addTab("http://mochi.test:8888/");
    let browser = gBrowser.getBrowserForTab(tab);
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);

      if (++numberOfTabsLoaded == maxTabsUndoPlusOne)
        verifyUndoMultipleClose();
    }, true);
  }
}

function verifyUndoMultipleClose() {
  info("all tabs opened and loaded");
  cancelRemoveAllTabsDialogListener = new WindowListener("chrome://global/content/commonDialog.xul", cancelRemoveAllTabsDialog);
  Services.wm.addListener(cancelRemoveAllTabsDialogListener);
  gBrowser.removeAllTabsBut(originalTab);
}

function cancelRemoveAllTabsDialog(domWindow) {
  ok(true, "dialog appeared in response to multiple tab close action");
  domWindow.document.documentElement.cancelDialog();
  Services.wm.removeListener(cancelRemoveAllTabsDialogListener);

  acceptRemoveAllTabsDialogListener = new WindowListener("chrome://global/content/commonDialog.xul", acceptRemoveAllTabsDialog);
  Services.wm.addListener(acceptRemoveAllTabsDialogListener);
  waitForCondition(function () gBrowser.tabs.length == 1 + maxTabsUndoPlusOne, function verifyCancel() {
    is(gBrowser.tabs.length, 1 + maxTabsUndoPlusOne, /* The '1 +' is for the original tab */
       "All tabs should still be open after the 'Cancel' option on the prompt is chosen");
    gBrowser.removeAllTabsBut(originalTab);
  }, "Waited too long to find that no tabs were closed.");
}

function acceptRemoveAllTabsDialog(domWindow) {
  ok(true, "dialog appeared in response to multiple tab close action");
  domWindow.document.documentElement.acceptDialog();
  Services.wm.removeListener(acceptRemoveAllTabsDialogListener);

  waitForCondition(function () gBrowser.tabs.length == 1, function verifyAccept() {
    is(gBrowser.tabs.length, 1,
       "All other tabs should be closed after the 'OK' option on the prompt is chosen");
    finish();
  }, "Waited too long for the other tabs to be closed.");
}

function WindowListener(aURL, aCallback) {
  this.callback = aCallback;
  this.url = aURL;
}
WindowListener.prototype = {
  onOpenWindow: function(aXULWindow) {
    var domWindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindow);
    var self = this;
    domWindow.addEventListener("load", function() {
      domWindow.removeEventListener("load", arguments.callee, false);

      info("domWindow.document.location.href: " + domWindow.document.location.href);
      if (domWindow.document.location.href != self.url)
        return;

      // Allow other window load listeners to execute before passing to callback
      executeSoon(function() {
        self.callback(domWindow);
      });
    }, false);
  },
  onCloseWindow: function(aXULWindow) {},
  onWindowTitleChange: function(aXULWindow, aNewTitle) {}
}
