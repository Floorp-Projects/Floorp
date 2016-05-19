/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the function searching works with pretty printed sources.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    Task.spawn(function* () {
      yield waitForSourceShown(gPanel, "code_ugly.js");

      let popupShown = promise.defer();
      once(gDebugger, "popupshown").then(() => {
        ok(isCaretPos(gPanel, 2, 10),
           "The bar function's non-pretty-printed location should be shown.");
        popupShown.resolve();
      });
      setText(gSearchBox, "@bar");
      yield popupShown.promise;

      const finished = waitForSourceShown(gPanel, "code_ugly.js");
      gDebugger.document.getElementById("pretty-print").click();
      yield finished;

      popupShown = promise.defer();
      once(gDebugger, "popupshown").then(() => {
        ok(isCaretPos(gPanel, 6, 10),
           "The bar function's pretty printed location should be shown.");
        popupShown.resolve();
      });
      setText(gSearchBox, "@bar");
      yield popupShown.promise;

      closeDebuggerAndFinish(gPanel);
    });
  });
}
