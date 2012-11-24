/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the debugger does show up even if a progress listener reads the
// WebProgress argument's DOMWindow property in onStateChange() (bug 771655).

var gPane = null;
var gTab = null;
var gOldListener = null;

const TEST_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

function test() {
  installListener();

  debug_tab_pane(TEST_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    let gDebugger = gPane.panelWin;

    is(gDebugger.DebuggerController._isInitialized, true,
      "Controller should be initialized after debug_tab_pane.");
    is(gDebugger.DebuggerView._isInitialized, true,
      "View should be initialized after debug_tab_pane.");

    closeDebuggerAndFinish();
  });
}

// This is taken almost verbatim from bug 771655.
function installListener() {
  if ("_testPL" in window) {
    gOldListener = _testPL;
    Cc['@mozilla.org/docloaderservice;1'].getService(Ci.nsIWebProgress)
                                         .removeProgressListener(_testPL);
  }

  window._testPL = {
    START_DOC: Ci.nsIWebProgressListener.STATE_START |
               Ci.nsIWebProgressListener.STATE_IS_DOCUMENT,
    onStateChange: function(wp, req, stateFlags, status) {
      if ((stateFlags & this.START_DOC) === this.START_DOC) {
        // This DOMWindow access triggers the unload event.
        wp.DOMWindow;
      }
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsISupportsWeakReference) ||
          iid.equals(Ci.nsIWebProgressListener))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  }

  Cc['@mozilla.org/docloaderservice;1'].getService(Ci.nsIWebProgress)
    .addProgressListener(_testPL, Ci.nsIWebProgress.NOTIFY_STATE_REQUEST);
}

registerCleanupFunction(function() {
  if (gOldListener) {
    window._testPL = gOldListener;
  } else {
    delete window._testPL;
  }
  removeTab(gTab);
  gPane = null;
  gTab = null;
});
