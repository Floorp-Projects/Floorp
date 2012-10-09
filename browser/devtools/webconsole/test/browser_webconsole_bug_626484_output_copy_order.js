/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
let itemsSet, HUD, outputNode;

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 626484");
  browser.addEventListener("load", function tabLoaded(aEvent) {
    browser.removeEventListener(aEvent.type, tabLoaded, true);
    openConsole(null, consoleOpened);
  }, true);
}


function consoleOpened(aHud) {
  HUD = aHud;
  outputNode = HUD.outputNode;
  HUD.jsterm.clearOutput();

  let console = content.wrappedJSObject.console;
  console.log("The first line.");
  console.log("The second line.");
  console.log("The last line.");
  itemsSet = [[0, 1, 2], [0, 2, 1], [1, 0, 2], [1, 2, 0], [2, 0, 1],
    [2, 1, 0]];

  waitForSuccess({
    name: "console.log messages displayed",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".hud-log").length == 3;
    },
    successFn: nextTest,
    failureFn: finishTest,
  });
}

function nextTest() {
  if (itemsSet.length === 0) {
    outputNode.clearSelection();
    HUD.jsterm.clearOutput();
    HUD = outputNode = null;
    executeSoon(finishTest);
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
      WCU_l10n.timestampString(item.timestamp) + "] " +
      item.clipboardText);
  }
  return expectedClipboardText.join("\n");
}

function clipboardSetup() {
  goDoCommand("cmd_copy");
}

