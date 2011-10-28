/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// For more information on GCLI see:
// - https://github.com/mozilla/gcli/blob/master/docs/index.md
// - https://wiki.mozilla.org/DevTools/Features/GCLI

// Tests that the inspect command works as it should

Components.utils.import("resource:///modules/gcli.jsm");

registerCleanupFunction(function() {
  gcliterm = undefined;
  requisition = undefined;

  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", true);
  addTab("http://example.com/browser/browser/devtools/webconsole/test/browser/browser_gcli_inspect.html");
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
  }
  finally {
    closeConsole();
    finishTest();
  }
}

function testSetup() {
  ok(gcliterm, "We have a GCLI term");
  ok(requisition, "We have a Requisition");
}

function testCreateCommands() {
  type("inspec");
  is(gcliterm.completeNode.textContent, " inspect", "Completion for \"inspec\"");
  is(requisition.getStatus().toString(), "ERROR", "inspec is ERROR");

  type("inspect");
  is(requisition.getStatus().toString(), "ERROR", "inspect is ERROR");

  type("inspect h1");
  is(requisition.getStatus().toString(), "ERROR", "inspect h1 is ERROR");

  type("inspect span");
  is(requisition.getStatus().toString(), "ERROR", "inspect span is ERROR");

  type("inspect div");
  is(requisition.getStatus().toString(), "VALID", "inspect div is VALID");

  type("inspect .someclass");
  is(requisition.getStatus().toString(), "VALID", "inspect .someclass is VALID");

  type("inspect #someid");
  is(requisition.getStatus().toString(), "VALID", "inspect #someid is VALID");

  type("inspect button[disabled]");
  is(requisition.getStatus().toString(), "VALID", "inspect button[disabled] is VALID");

  type("inspect p>strong");
  is(requisition.getStatus().toString(), "VALID", "inspect p>strong is VALID");

  type("inspect :root");
  is(requisition.getStatus().toString(), "VALID", "inspect :root is VALID");
}

function type(command) {
  gcliterm.inputNode.value = command.slice(0, -1);
  gcliterm.inputNode.focus();
  EventUtils.synthesizeKey(command.slice(-1), {});
}
