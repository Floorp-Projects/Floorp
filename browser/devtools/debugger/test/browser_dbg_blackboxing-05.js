/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a stack frame for each black boxed source, not a single one
 * for all of them.
 */

const TAB_URL = EXAMPLE_URL + "binary_search.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let resumed = false;
  let testStarted = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    resumed = true;
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    once(gDebugger, "Debugger:SourceShown", testSourceEditorShown);
  });
}

function testSourceEditorShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, "0",
     "The first item in the deck should be selected (the source editor)");
  blackBoxSource();
}

function blackBoxSource() {
  const { activeThread } = gDebugger.DebuggerController;
  activeThread.addOneTimeListener("blackboxchange", testBlackBoxMessageShown);
  getAnyBlackBoxCheckbox().click();
}

function testBlackBoxMessageShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, "1",
     "The second item in the deck should be selected (the black box message)");
  clickStopBlackBoxingButton();
}

function clickStopBlackBoxingButton() {
  const button = gDebugger.document.getElementById("black-boxed-message-button");
  const { activeThread } = gDebugger.DebuggerController;
  activeThread.addOneTimeListener("blackboxchange", testSourceEditorShownAgain);
  button.click();
}

function testSourceEditorShownAgain() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, "0",
     "The first item in the deck should be selected again (the source editor)");
  closeDebuggerAndFinish();
}

function getAnyBlackBoxCheckbox() {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item .side-menu-widget-item-checkbox");
}

function once(target, event, callback) {
  target.addEventListener(event, function _listener(...args) {
    target.removeEventListener(event, _listener, false);
    callback.apply(null, args);
  }, false);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
