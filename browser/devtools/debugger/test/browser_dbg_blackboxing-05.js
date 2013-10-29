/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that a "this source is blackboxed" message is shown when necessary
 * and can be properly dismissed.
 */

const TAB_URL = EXAMPLE_URL + "doc_binary_search.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gDeck;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gDeck = gDebugger.document.getElementById("editor-deck");

    waitForSourceShown(gPanel, ".coffee")
      .then(testSourceEditorShown)
      .then(toggleBlackBoxing.bind(null, gPanel))
      .then(testBlackBoxMessageShown)
      .then(clickStopBlackBoxingButton)
      .then(testSourceEditorShownAgain)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testSourceEditorShown() {
  is(gDeck.selectedIndex, "0",
    "The first item in the deck should be selected (the source editor).");
}

function testBlackBoxMessageShown() {
  is(gDeck.selectedIndex, "1",
    "The second item in the deck should be selected (the black box message).");
}

function clickStopBlackBoxingButton() {
  let finished = waitForThreadEvents(gPanel, "blackboxchange");
  getEditorBlackboxMessageButton().click();
  return finished;
}

function testSourceEditorShownAgain() {
  is(gDeck.selectedIndex, "0",
    "The first item in the deck should be selected again (the source editor).");
}

function getEditorBlackboxMessageButton() {
  return gDebugger.document.getElementById("black-boxed-message-button");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gDeck = null;
});
