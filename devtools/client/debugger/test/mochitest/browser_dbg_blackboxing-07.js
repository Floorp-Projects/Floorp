/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that if we unblackbox a source which has been automatically blackboxed
 * and then refresh, it is still unblackboxed.
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing_unblackbox.html";

var gTab, gPanel, gDebugger;

function test() {
  let options = {
    source: EXAMPLE_URL + "code_blackboxing_unblackbox.min.js",
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testBlackBoxSource()
      .then(testBlackBoxReload)
      .then(() => closeDebuggerAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testBlackBoxSource() {
  const bbButton = getBlackBoxButton(gPanel);
  ok(bbButton.checked, "Should be black boxed by default");

  return toggleBlackBoxing(gPanel).then(aSource => {
    ok(!aSource.isBlackBoxed, "The source should no longer be blackboxed.");
  });
}

function testBlackBoxReload() {
  return reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(() => {
    const selectedSource = getSelectedSourceElement(gPanel);
    ok(!selectedSource.isBlackBoxed, "The source should not be blackboxed.");
  });
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
