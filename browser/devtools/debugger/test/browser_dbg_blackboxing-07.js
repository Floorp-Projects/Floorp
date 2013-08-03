/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that clicking the black box checkbox when paused doesn't re-select the
 * currently paused frame's source.
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

    once(gDebugger, "Debugger:SourceShown", runTest);
  });
}

function runTest() {
  const { activeThread } = gDebugger.DebuggerController;
  activeThread.addOneTimeListener("paused", function () {
    const sources = gDebugger.DebuggerView.Sources;
    const selectedUrl = sources.selectedItem.attachment.source.url;

    once(gDebugger, "Debugger:SourceShown", function () {
      const newSelectedUrl = sources.selectedItem.attachment.source.url;
      isnot(selectedUrl, newSelectedUrl,
            "Should not have the same url selected");

      activeThread.addOneTimeListener("blackboxchange", function () {
        isnot(sources.selectedItem.attachment.source.url,
              selectedUrl,
              "The selected source did not change");
        closeDebuggerAndFinish();
      });

      getBlackBoxCheckbox(newSelectedUrl).click();
    });

    getDifferentSource(selectedUrl).click();
  });

  gDebuggee.runTest();
}

function getDifferentSource(url) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item:not([tooltiptext=\""
      + url + "\"])");
}

function getBlackBoxCheckbox(url) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item[tooltiptext=\""
      + url + "\"] .side-menu-widget-item-checkbox");
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
