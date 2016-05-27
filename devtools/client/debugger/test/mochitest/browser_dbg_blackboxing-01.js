/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that if we black box a source and then refresh, it is still black boxed.
 */

const TAB_URL = EXAMPLE_URL + "doc_binary_search.html";

var gTab, gPanel, gDebugger;

function test() {
  let options = {
    source: ".coffee",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testBlackBoxSource()
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

  return toggleBlackBoxing(gPanel).then(source => {
    ok(source.isBlackBoxed, "The source should be black boxed now.");
    ok(bbButton.checked, "The checkbox should no longer be checked.");
  });
}

function testBlackBoxReload() {
  return reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(() => {
    const bbButton = getBlackBoxButton(gPanel);
    const selectedSource = getSelectedSourceElement(gPanel);
    ok(bbButton.checked, "Should still be black boxed.");
    ok(selectedSource.classList.contains("black-boxed"),
      "'black-boxed' class should still be applied");
  });
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
