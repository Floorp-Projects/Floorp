function numClosedTabs()
  Cc["@mozilla.org/browser/sessionstore;1"].
    getService(Ci.nsISessionStore).
    getNumberOfTabsClosedLast(window);

const kPromptServiceUUID = "{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}";
const kPromptServiceContractID = "@mozilla.org/embedcomp/prompt-service;1";
const Cm = Components.manager;

// Save original prompt service factory
const kPromptServiceFactory = Cm.getClassObject(Cc[kPromptServiceContractID],
                                                Ci.nsIFactory);

let fakePromptServiceFactory = {
  createInstance: function(aOuter, aIid) {
    if (aOuter != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return promptService.QueryInterface(aIid);
  }
};

// This will first return 1 as the button pressed (cancel), and
// all subsequent calls will return 0 (accept).
let buttonPressed = 1;
let promptService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptService]),
  confirmEx: function() {
    return Math.max(buttonPressed--, 0);
  }
};

Cm.QueryInterface(Ci.nsIComponentRegistrar)
  .registerFactory(Components.ID(kPromptServiceUUID), "Prompt Service",
                   kPromptServiceContractID, fakePromptServiceFactory);

let originalTab;
let maxTabsUndo;
let maxTabsUndoPlusOne;

function verifyUndoMultipleClose() {
  gBrowser.removeAllTabsBut(originalTab);
  is(gBrowser.tabs.length, 1 + maxTabsUndoPlusOne, /* The '1 +' is for the original tab */
     "All tabs should still be open when the 'Cancel' option on the prompt is chosen");
  gBrowser.removeAllTabsBut(originalTab);
  is(gBrowser.tabs.length, 1,
     "All other tabs should be closed when the 'OK' option on the prompt is chosen");
  finish();
}

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref("browser.tabs.animate", false);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.tabs.animate");

    // Unregister the factory so we do not leak
    Cm.QueryInterface(Ci.nsIComponentRegistrar)
      .unregisterFactory(Components.ID(kPromptServiceUUID),
                         fakePromptServiceFactory);

    // Restore the original factory
    Cm.QueryInterface(Ci.nsIComponentRegistrar)
      .registerFactory(Components.ID(kPromptServiceUUID), "Prompt Service",
                       kPromptServiceContractID, kPromptServiceFactory);

    originalTab.linkedBrowser.loadURI("about:blank");
    originalTab = null;
  });

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
