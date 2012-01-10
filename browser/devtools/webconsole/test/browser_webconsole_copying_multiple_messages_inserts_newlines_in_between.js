/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that copying multiple messages inserts newlines in between.

const TEST_URI = "data:text/html,Web Console test for bug 586142";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test()
{
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad,
                                               false);
  executeSoon(testNewlines);
}

function testNewlines() {
  openConsole();
  hud = HUDService.getHudByWindow(content);
  hud.jsterm.clearOutput();

  let console = content.wrappedJSObject.console;
  ok(console != null, "we have the console object");

  for (let i = 0; i < 20; i++) {
    console.log("Hello world!");
  }

  let outputNode = hud.outputNode;

  outputNode.selectAll();
  outputNode.focus();

  let clipboardTexts = [];
  for (let i = 0; i < outputNode.itemCount; i++) {
    let item = outputNode.getItemAtIndex(i);
    clipboardTexts.push("[" + ConsoleUtils.timestampString(item.timestamp) +
                        "] " + item.clipboardText);
  }

  waitForClipboard(clipboardTexts.join("\n"),
                   function() { goDoCommand("cmd_copy"); },
                   finishTest, finishTest);
}

