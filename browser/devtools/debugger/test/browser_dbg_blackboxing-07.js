/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that clicking the black box checkbox when paused doesn't re-select the
 * currently paused frame's source.
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 21)
      .then(testBlackBox)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.runTest();
  });
}

function testBlackBox() {
  const selectedUrl = gSources.selectedValue;

  let finished = waitForSourceShown(gPanel, "blackboxme.js").then(() => {
    const newSelectedUrl = gSources.selectedValue;
    isnot(selectedUrl, newSelectedUrl,
      "Should not have the same url selected.");

    let finished = waitForThreadEvents(gPanel, "blackboxchange").then(() => {
      is(gSources.selectedValue, newSelectedUrl,
        "The selected source did not change.");
    });

    getBlackBoxCheckbox(newSelectedUrl).click()
    return finished;
  });

  gSources.selectedIndex = 0;
  return finished;
}

function getBlackBoxCheckbox(aUrl) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item[tooltiptext=\"" + aUrl + "\"] " +
    ".side-menu-widget-item-checkbox");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
