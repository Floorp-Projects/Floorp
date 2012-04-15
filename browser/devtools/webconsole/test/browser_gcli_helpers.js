/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// For more information on GCLI see:
// - https://github.com/mozilla/gcli/blob/master/docs/index.md
// - https://wiki.mozilla.org/DevTools/Features/GCLI

let tmp = {};
Components.utils.import("resource:///modules/gcli.jsm", tmp);
let gcli = tmp.gcli;

let hud;
let gcliterm;

registerCleanupFunction(function() {
  gcliterm = undefined;
  hud = undefined;
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", true);
  addTab("http://example.com/browser/browser/devtools/webconsole/test/test-console.html");
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole();
  hud = HUDService.getHudByWindow(content);
  gcliterm = hud.gcliterm;

  testHelpers();
  testScripts();

  closeConsole();
  finishTest();

  // gcli._internal.console.error("Command Tests Completed");
}

function testScripts() {
  check("{ 'id=' + $('header').getAttribute('id')", '"id=header"');
  check("{ headerQuery = $$('h1')", "Instance of NodeList");
  check("{ 'length=' + headerQuery.length", '"length=1"');

  check("{ xpathQuery = $x('.//*', document.body);", 'Instance of Array');
  check("{ 'headerFound='  + (xpathQuery[0] == headerQuery[0])", '"headerFound=true"');

  check("{ 'keysResult=' + (keys({b:1})[0] == 'b')", '"keysResult=true"');
  check("{ 'valuesResult=' + (values({b:1})[0] == 1)", '"valuesResult=true"');

  check("{ [] instanceof Array", "true");
  check("{ ({}) instanceof Object", "true");
  check("{ document", "Instance of HTMLDocument");
  check("{ null", "null");
  check("{ undefined", undefined);

  check("{ for (var x=0; x<9; x++) x;", "8");
}

function check(command, reply) {
  gcliterm.clearOutput();
  gcliterm.opts.console.inputter.setInput(command);
  gcliterm.opts.requisition.exec();

  let labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
  if (reply === undefined) {
    is(labels.length, 0, "results count for: " + command);
  }
  else {
    is(labels.length, 1, "results count for: " + command);
    is(labels[0].textContent.trim(), reply, "message for: " + command);
  }

  gcliterm.opts.console.inputter.setInput("");
}

function testHelpers() {
  gcliterm.clearOutput();
  gcliterm.opts.console.inputter.setInput("{ pprint({b:2, a:1})");
  gcliterm.opts.requisition.exec();
  // Doesn't conform to check() format
  let label = hud.outputNode.querySelector(".webconsole-msg-output");
  is(label.textContent.trim(), "a: 1\n  b: 2", "pprint() worked");

  // no gcliterm.clearOutput() here as we clear the output using the clear() fn.
  gcliterm.opts.console.inputter.setInput("{ clear()");
  gcliterm.opts.requisition.exec();
  ok(!hud.outputNode.querySelector(".hud-group"), "clear() worked");

  // check that pprint(window) and keys(window) don't throw, bug 608358
  gcliterm.clearOutput();
  gcliterm.opts.console.inputter.setInput("{ pprint(window)");
  gcliterm.opts.requisition.exec();
  let labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
  is(labels.length, 1, "one line of output for pprint(window)");

  gcliterm.clearOutput();
  gcliterm.opts.console.inputter.setInput("{ keys(window)");
  gcliterm.opts.requisition.exec();
  labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
  is(labels.length, 1, "one line of output for keys(window)");

  gcliterm.clearOutput();
  gcliterm.opts.console.inputter.setInput("{ pprint('hi')");
  gcliterm.opts.requisition.exec();
  // Doesn't conform to check() format, bug 614561
  label = hud.outputNode.querySelector(".webconsole-msg-output");
  is(label.textContent.trim(), '0: "h"\n  1: "i"', 'pprint("hi") worked');

  // Causes a memory leak. FIXME Bug 717892
  /*
  // check that pprint(function) shows function source, bug 618344
  gcliterm.clearOutput();
  gcliterm.opts.console.inputter.setInput("{ pprint(print)");
  gcliterm.opts.requisition.exec();
  label = hud.outputNode.querySelector(".webconsole-msg-output");
  isnot(label.textContent.indexOf("SEVERITY_LOG"), -1, "pprint(function) shows function source");
  */

  gcliterm.clearOutput();
}
