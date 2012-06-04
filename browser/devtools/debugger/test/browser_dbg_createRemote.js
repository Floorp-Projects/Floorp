/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that a remote debugger can be created in a new window.

var gWindow = null;
var gTab = null;
var gRemotePort = null;
var gAutoConnect = null;

const TEST_URL = EXAMPLE_URL + "browser_dbg_iframes.html";

function test() {
  debug_remote(TEST_URL, function(aTab, aDebuggee, aWindow) {
    gTab = aTab;
    gWindow = aWindow;
    let gDebugger = gWindow.contentWindow;

    info("Current remote port: " +
      Services.prefs.getIntPref("devtools.debugger.remote-port"));
    info("Current autoconnect flag: " +
      Services.prefs.getBoolPref("devtools.debugger.remote-autoconnect"));

    is(gDebugger.document.getElementById("close").getAttribute("hidden"), "true",
      "The close button should be hidden in a remote debugger.");

    is(gDebugger.DebuggerController.activeThread.paused, false,
      "Should be running after debug_remote.");

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      Services.tm.currentThread.dispatch({ run: function() {

        let frames = gDebugger.DebuggerView.StackFrames._frames;
        let childNodes = frames.childNodes;

        is(gDebugger.DebuggerController.activeThread.paused, true,
          "Should be paused after an interrupt request.");

        is(frames.querySelectorAll(".dbg-stackframe").length, 1,
          "Should have one frame in the stack.");

        gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
          Services.tm.currentThread.dispatch({ run: function() {
            closeDebuggerAndFinish(true);
          }}, 0);
        });

        EventUtils.sendMouseEvent({ type: "click" },
          gDebugger.document.getElementById("resume"),
          gDebugger);
      }}, 0);
    });

    let iframe = gTab.linkedBrowser.contentWindow.wrappedJSObject.frames[0];

    is(iframe.document.title, "Browser Debugger Test Tab", "Found the iframe");

    iframe.runDebuggerStatement();
  },
  function beforeTabAdded() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init(function() { return true; });
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.closeListener();

    gRemotePort = Services.prefs.getIntPref("devtools.debugger.remote-port");
    gAutoConnect = Services.prefs.getBoolPref("devtools.debugger.remote-autoconnect");

    // Open the listener at some point in the future to test automatic reconnect.
    openListener(gRemotePort + 1);
  });
}

let attempts = 0;

function openListener(port) {
  Services.prefs.setIntPref("devtools.debugger.remote-port", port);
  Services.prefs.setBoolPref("devtools.debugger.remote-autoconnect", true);

  info("Attempting to open a new listener on port " + port);
  try {
    info("Closing listener...");
    DebuggerServer.closeListener();
    info("Opening listener...");
    DebuggerServer.openListener(port);
  } catch (e) {
    info(e);
    info("Exception caught when opening listener on port " + port);
    info("Retrying with port " + (++port));

    if (++attempts < 100) {
      DebuggerServer.closeListener();
      openListener(port);
    } else {
      ok(false, "Timed out while opening a listener.");
    }
  }
}

registerCleanupFunction(function() {
  Services.prefs.setIntPref("devtools.debugger.remote-port", gRemotePort);
  Services.prefs.setBoolPref("devtools.debugger.remote-autoconnect", gAutoConnect);
  removeTab(gTab);
  gWindow = null;
  gTab = null;
  gRemotePort = null;
  gAutoConnect = null;
});
