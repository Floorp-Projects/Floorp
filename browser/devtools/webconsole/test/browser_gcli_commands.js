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
  addTab("http://example.com/browser/browser/devtools/webconsole/test/browser_gcli_inspect.html");
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole();

  hud = HUDService.getHudByWindow(content);
  gcliterm = hud.gcliterm;

  testEcho();

  // gcli._internal.console.error("Command Tests Completed");
}

function testEcho() {
  let nodes = exec("echo message");
  is(nodes.length, 2, "after echo");
  is(nodes[0].textContent, "echo message", "output 0");
  is(nodes[1].textContent.trim(), "message", "output 1");

  testConsoleClear();
}

function testConsoleClear() {
  let nodes = exec("console clear");
  is(nodes.length, 1, "after console clear 1");

  executeSoon(function() {
    let nodes = hud.outputNode.querySelectorAll("richlistitem");
    is(nodes.length, 0, "after console clear 2");

    testConsoleClose();
  });
}

function testConsoleClose() {
  ok(hud.hudId in HUDService.hudReferences, "console open");

  exec("console close");

  ok(!(hud.hudId in HUDService.hudReferences), "console closed");

  finishTest();
}

function exec(command) {
  gcliterm.clearOutput();
  let nodes = hud.outputNode.querySelectorAll("richlistitem");
  is(nodes.length, 0, "setup - " + command);

  gcliterm.opts.console.inputter.setInput(command);
  gcliterm.opts.requisition.exec();

  return hud.outputNode.querySelectorAll("richlistitem");
}
