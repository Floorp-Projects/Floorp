/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that if we black box a source and then refresh, it is still black boxed.
 */

const TAB_URL = EXAMPLE_URL + "doc_binary_search.html";

let gTab, gDebuggee, gPanel, gDebugger;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    waitForSourceShown(gPanel, ".coffee")
      .then(testBlackBoxSource)
      .then(testBlackBoxReload)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testBlackBoxSource() {
  const bbButton = getBlackBoxButton(gPanel);
  ok(!bbButton.checked, "Should not be black boxed by default");

  return toggleBlackBoxing(gPanel).then(aSource => {
    ok(aSource.isBlackBoxed, "The source should be black boxed now.");
    ok(bbButton.checked, "The checkbox should no longer be checked.");
  });
}

function testBlackBoxReload() {
  return reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(() => {
    const bbButton = getBlackBoxButton(gPanel);
    ok(bbButton.checked, "Should still be black boxed.");
  });
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
