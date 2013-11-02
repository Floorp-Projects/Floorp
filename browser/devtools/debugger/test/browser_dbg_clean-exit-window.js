/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that closing a window with the debugger in a paused state exits cleanly.
 */

let gDebuggee, gPanel, gDebugger, gWindow;

const TAB_URL = EXAMPLE_URL + "doc_inline-debugger-statement.html";

function test() {
  addWindow(TAB_URL)
    .then(win => initDebugger(TAB_URL, win))
    .then(([aTab, aDebuggee, aPanel, aWindow]) => {
      gDebuggee = aDebuggee;
      gPanel = aPanel;
      gDebugger = gPanel.panelWin;
      gWindow = aWindow;

      return testCleanExit();
    })
    .then(null, aError => {
      ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    });
}

function testCleanExit() {
  let deferred = promise.defer();

  ok(!!gWindow, "Second window created.");

  gWindow.focus();

  is(Services.wm.getMostRecentWindow("navigator:browser"), gWindow,
    "The second window is on top.");

  let isActive = promise.defer();
  let isLoaded = promise.defer();

  promise.all([isActive.promise, isLoaded.promise]).then(() => {
    gWindow.BrowserChromeTest.runWhenReady(() => {
      waitForSourceAndCaretAndScopes(gPanel, ".html", 16).then(() => {
        is(gDebugger.gThreadClient.paused, true,
          "Should be paused after the debugger statement.");
        gWindow.close();
        deferred.resolve();
        finish();
      });

      gDebuggee.runDebuggerStatement();
    });
  });

  let focusManager = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  if (focusManager.activeWindow != gWindow) {
    gWindow.addEventListener("activate", function onActivate(aEvent) {
      if (aEvent.target != gWindow) {
        return;
      }
      gWindow.removeEventListener("activate", onActivate, true);
      isActive.resolve();
    }, true);
  } else {
    isActive.resolve();
  }

  if (gWindow.content.location.href != TAB_URL) {
    gWindow.document.addEventListener("load", function onLoad(aEvent) {
      if (aEvent.target.documentURI != TAB_URL) {
        return;
      }
      gWindow.document.removeEventListener("load", onLoad, true);
      isLoaded.resolve();
    }, true);
  } else {
    isLoaded.resolve();
  }
  return deferred.promise;
}

registerCleanupFunction(function() {
  gWindow = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
