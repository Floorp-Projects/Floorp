/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
let itemsSet, HUD;

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab("data:text/html,Web Console test for bug 626484");
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);
  openConsole();

  let console = browser.contentWindow.wrappedJSObject.console;
  console.log("The first line.");
  console.log("The second line.");
  console.log("The last line.");

  let hudId = HUDService.getHudIdByWindow(content);
  HUD = HUDService.hudReferences[hudId];
  outputNode = HUD.outputNode;

  itemsSet = [[0, 1, 2], [0, 2, 1], [1, 0, 2], [1, 2, 0], [2, 0, 1],
    [2, 1, 0]];

  nextTest();
}

function nextTest() {
  if (itemsSet.length === 0) {
    outputNode.clearSelection();
    HUD.jsterm.clearOutput();
    HUD = null;
    finish();
  }
  else {
    outputNode.clearSelection();
    let items = itemsSet.shift();
    items.forEach(function (index) {
      outputNode.addItemToSelection(outputNode.getItemAtIndex(index));
    });
    outputNode.focus();
    waitForClipboard(getExpectedClipboardText(items.length),
      clipboardSetup, nextTest, nextTest);
  }
}

function getExpectedClipboardText(aItemCount) {
  let expectedClipboardText = [];
  for (let i = 0; i < aItemCount; i++) {
    let item = outputNode.getItemAtIndex(i);
    expectedClipboardText.push("[" +
      ConsoleUtils.timestampString(item.timestamp) + "] " +
      item.clipboardText);
  }
  return expectedClipboardText.join("\n");
}

function clipboardSetup() {
  goDoCommand("cmd_copy");
}

