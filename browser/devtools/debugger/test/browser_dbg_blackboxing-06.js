/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that clicking the black box checkbox doesn't select that source.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_blackboxing.html";

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

    once(gDebugger, "Debugger:SourceShown", testBlackBox);
  });
}

function testBlackBox() {
  const sources = gDebugger.DebuggerView.Sources;

  const selectedUrl = sources.selectedItem.attachment.source.url;
  const checkbox = getDifferentBlackBoxCheckbox(selectedUrl);
  ok(checkbox, "We should be able to grab a checkbox");

  const { activeThread } = gDebugger.DebuggerController;
  activeThread.addOneTimeListener("blackboxchange", function () {
    is(selectedUrl,
       sources.selectedItem.attachment.source.url,
       "The same source should be selected");
    closeDebuggerAndFinish();
  });

  checkbox.click();
}

function getDifferentBlackBoxCheckbox(url) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item:not([tooltiptext=\""
      + url + "\"]) .side-menu-widget-item-checkbox");
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
