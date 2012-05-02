/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// For more information on GCLI see:
// - https://github.com/mozilla/gcli/blob/master/docs/index.md
// - https://wiki.mozilla.org/DevTools/Features/GCLI

// Tests that the break command works as it should

let tempScope = {};
Components.utils.import("resource:///modules/gcli.jsm", tempScope);
let gcli = tempScope.gcli;

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/browser_gcli_break.html";
registerCleanupFunction(function() {
  gcliterm = undefined;
  requisition = undefined;

  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", true);
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

let gcliterm;
let requisition;

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  try {
    openConsole();

    let hud = HUDService.getHudByWindow(content);
    gcliterm = hud.gcliterm;
    requisition = gcliterm.opts.requisition;

    testSetup();
    testCreateCommands();
  }
  catch (ex) {
    ok(false, "Caught exception: " + ex)
    gcli._internal.console.error("Test Failure", ex);
    closeConsole();
    finishTest();
  }
}

function testSetup() {
  ok(gcliterm, "We have a GCLI term");
  ok(requisition, "We have a Requisition");
}

function testCreateCommands() {
  type("brea");
  is(gcliterm.completeNode.textContent, " break", "Completion for 'brea'");
  is(requisition.getStatus().toString(), "ERROR", "brea is ERROR");

  type("break");
  is(requisition.getStatus().toString(), "ERROR", "break is ERROR");

  type("break add");
  is(requisition.getStatus().toString(), "ERROR", "break add is ERROR");

  type("break add line");
  is(requisition.getStatus().toString(), "ERROR", "break add line is ERROR");

  let pane = DebuggerUI.toggleDebugger();
  pane._frame.addEventListener("Debugger:Connecting", function dbgConnected() {
    pane._frame.removeEventListener("Debugger:Connecting", dbgConnected, true);

    // Wait for the initial resume.
    pane.debuggerWindow.gClient.addOneTimeListener("resumed", function() {
      pane.debuggerWindow.gClient.activeThread.addOneTimeListener("framesadded", function() {
        type("break add line " + TEST_URI + " " + content.wrappedJSObject.line0);
        is(requisition.getStatus().toString(), "VALID", "break add line is VALID");
        requisition.exec();

        type("break list");
        is(requisition.getStatus().toString(), "VALID", "break list is VALID");
        requisition.exec();

        pane.debuggerWindow.gClient.activeThread.resume(function() {
          type("break del 0");
          is(requisition.getStatus().toString(), "VALID", "break del 0 is VALID");
          requisition.exec();

          closeConsole();
          finishTest();
        });
      });
      // Trigger newScript notifications using eval.
      content.wrappedJSObject.firstCall();
    });
  }, true);
}

function type(command) {
  gcliterm.inputNode.value = command.slice(0, -1);
  gcliterm.inputNode.focus();
  EventUtils.synthesizeKey(command.slice(-1), {});
}
