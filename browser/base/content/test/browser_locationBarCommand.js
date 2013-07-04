/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_VALUE = "example.com";
const START_VALUE = "example.org";

let gFocusManager = Cc["@mozilla.org/focus-manager;1"].
                    getService(Ci.nsIFocusManager);

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.altClickSave");
  });
  Services.prefs.setBoolPref("browser.altClickSave", true);

  runAltLeftClickTest();
}

// Monkey patch saveURL to avoid dealing with file save code paths
var oldSaveURL = saveURL;
saveURL = function() {
  ok(true, "SaveURL was called");
  is(gURLBar.value, "", "Urlbar reverted to original value");
  saveURL = oldSaveURL;
  runShiftLeftClickTest();
}
function runAltLeftClickTest() {
  info("Running test: Alt left click");
  triggerCommand(true, { altKey: true });
}

function runShiftLeftClickTest() {
  let listener = new WindowListener(getBrowserURL(), function(aWindow) {
    Services.wm.removeListener(listener);
    addPageShowListener(aWindow.gBrowser.selectedBrowser, function() {
      info("URL should be loaded in a new window");
      is(gURLBar.value, "", "Urlbar reverted to original value");       
      is(gFocusManager.focusedElement, null, "There should be no focused element");
      is(gFocusManager.focusedWindow, aWindow.gBrowser.contentWindow, "Content window should be focused");
      is(aWindow.gURLBar.value, TEST_VALUE, "New URL is loaded in new window");

      aWindow.close();
      runNextTest();
    }, "http://example.com/");
  });
  Services.wm.addListener(listener);

  info("Running test: Shift left click");
  triggerCommand(true, { shiftKey: true });
}

function runNextTest() {
  let test = gTests.shift();
  if (!test) {
    finish();
    return;
  }
  
  info("Running test: " + test.desc);
  // Tab will be blank if test.startValue is null
  let tab = gBrowser.selectedTab = gBrowser.addTab(test.startValue);
  addPageShowListener(gBrowser.selectedBrowser, function() {
    triggerCommand(test.click, test.event);
    test.check(tab);

    // Clean up
    while (gBrowser.tabs.length > 1)
      gBrowser.removeTab(gBrowser.selectedTab)
    runNextTest();
  });
}

let gTests = [
  { desc: "Right click on go button",
    click: true,
    event: { button: 2 },
    check: function(aTab) {
      // Right click should do nothing (context menu will be shown)
      is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");
    }
  },

  { desc: "Left click on go button",
    click: true,
    event: {},
    check: checkCurrent
  },

  { desc: "Ctrl/Cmd left click on go button",
    click: true,
    event: { accelKey: true },
    check: checkNewTab
  },

  { desc: "Shift+Ctrl/Cmd left click on go button",
    click: true,
    event: { accelKey: true, shiftKey: true },
    check: function(aTab) {
      info("URL should be loaded in a new background tab");
      is(gURLBar.value, "", "Urlbar reverted to original value");
      ok(!gURLBar.focused, "Urlbar is no longer focused after urlbar command");
      is(gBrowser.selectedTab, aTab, "Focus did not change to the new tab");
    
      // Select the new background tab
      gBrowser.selectedTab = gBrowser.selectedTab.nextSibling;
      is(gURLBar.value, TEST_VALUE, "New URL is loaded in new tab");
    }
  },

  { desc: "Simple return keypress",
    event: {},
    check: checkCurrent
  },

  { desc: "Alt+Return keypress in a blank tab",
    event: { altKey: true },
    check: checkCurrent
  },

  { desc: "Alt+Return keypress in a dirty tab",
    event: { altKey: true },
    check: checkNewTab,
    startValue: START_VALUE
  },

  { desc: "Ctrl/Cmd+Return keypress",
    event: { accelKey: true },
    check: checkCurrent
  }
]

let gGoButton = document.getElementById("urlbar-go-button");
function triggerCommand(aClick, aEvent) {
  gURLBar.value = TEST_VALUE;
  gURLBar.focus();

  if (aClick) {
    is(gURLBar.getAttribute("pageproxystate"), "invalid",
       "page proxy state must be invalid for go button to be visible");
    EventUtils.synthesizeMouseAtCenter(gGoButton, aEvent); 
  }
  else
    EventUtils.synthesizeKey("VK_RETURN", aEvent);
}

/* Checks that the URL was loaded in the current tab */
function checkCurrent(aTab) {
  info("URL should be loaded in the current tab");
  is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");
  is(gFocusManager.focusedElement, null, "There should be no focused element");
  is(gFocusManager.focusedWindow, gBrowser.contentWindow, "Content window should be focused");
  is(gBrowser.selectedTab, aTab, "New URL was loaded in the current tab");
}

/* Checks that the URL was loaded in a new focused tab */
function checkNewTab(aTab) {
  info("URL should be loaded in a new focused tab");
  is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");
  is(gFocusManager.focusedElement, null, "There should be no focused element");
  is(gFocusManager.focusedWindow, gBrowser.contentWindow, "Content window should be focused");
  isnot(gBrowser.selectedTab, aTab, "New URL was loaded in a new tab");
}

function addPageShowListener(browser, cb, expectedURL) {
  browser.addEventListener("pageshow", function pageShowListener() {
    info("pageshow: " + browser.currentURI.spec);
    if (expectedURL && browser.currentURI.spec != expectedURL)
      return; // ignore pageshows for non-expected URLs
    browser.removeEventListener("pageshow", pageShowListener, false);
    cb();
  });
}

function WindowListener(aURL, aCallback) {
  this.callback = aCallback;
  this.url = aURL;
}
WindowListener.prototype = {
  onOpenWindow: function(aXULWindow) {
    var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindow);
    var self = this;
    domwindow.addEventListener("load", function() {
      domwindow.removeEventListener("load", arguments.callee, false);

      if (domwindow.document.location.href != self.url)
        return;

      // Allow other window load listeners to execute before passing to callback
      executeSoon(function() {
        self.callback(domwindow);
      });
    }, false);
  },
  onCloseWindow: function(aXULWindow) {},
  onWindowTitleChange: function(aXULWindow, aNewTitle) {}
}
