/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// For more information on GCLI see:
// - https://github.com/mozilla/gcli/blob/master/docs/index.md
// - https://wiki.mozilla.org/DevTools/Features/GCLI

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

Components.utils.import("resource:///modules/gcli.jsm");
let require = gcli._internal.require;

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/browser/test-console.html";

registerCleanupFunction(function() {
  require = undefined;
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", true);
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  try {
    openConsole();

    testCreateCommands();
    testCallCommands();
    testRemoveCommands();
  }
  catch (ex) {
    gcli._internal.console.error('Test Failure', ex);
    ok(false, '' + ex);
  }
  finally {
    closeConsole();
    finishTest();
  }
}

let tselarr = {
  name: 'tselarr',
  params: [
    { name: 'num', type: { name: 'selection', data: [ '1', '2', '3' ] } },
    { name: 'arr', type: { name: 'array', subtype: 'string' } },
  ],
  exec: function(args, env) {
    return "flu " + args.num + "-" + args.arr.join("_");
  }
};

function testCreateCommands() {
  let gcli = require("gcli/index");
  gcli.addCommand(tselarr);

  let canon = require("gcli/canon");
  let tselcmd = canon.getCommand("tselarr");
  ok(tselcmd != null, "tselarr exists in the canon");
  ok(tselcmd instanceof canon.Command, "canon storing commands");
}

function testCallCommands() {
  let hud = HUDService.getHudByWindow(content);
  let gcliterm = hud.gcliterm;
  ok(gcliterm, "We have a GCLI term");

  // Test successful auto-completion
  gcliterm.inputNode.value = "h";
  gcliterm.inputNode.focus();
  EventUtils.synthesizeKey("e", {});
  is(gcliterm.completeNode.textContent, " help", "Completion for \"he\"");

  // Test unsuccessful auto-completion
  gcliterm.inputNode.value = "ec";
  gcliterm.inputNode.focus();
  EventUtils.synthesizeKey("d", {});
  is(gcliterm.completeNode.textContent, " ecd", "Completion for \"ecd\"");

  // Test a normal command's life cycle
  gcliterm.opts.display.inputter.setInput("echo hello world");
  gcliterm.opts.requisition.exec();

  let nodes = hud.outputNode.querySelectorAll("description");

  is(nodes.length, 2, "Right number of output nodes");
  ok(/hello world/.test(nodes[0].textContent), "the command's output is correct.");

  gcliterm.clearOutput();
}

function testRemoveCommands() {
  let gcli = require("gcli/index");
  gcli.removeCommand(tselarr);

  let canon = require("gcli/canon");
  let tselcmd = canon.getCommand("tselarr");
  ok(tselcmd == null, "tselcmd removed from the canon");
}
