/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that keybindings still work when the content window is paused and
// the tab is selected again.

function test() {
  Task.spawn(function* () {
    const TAB_URL = EXAMPLE_URL + "doc_inline-script.html";
    let gDebugger, searchBox;

    let options = {
      source: TAB_URL,
      line: 1
    };
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL, options);
    gDebugger = panel.panelWin;
    searchBox = gDebugger.DebuggerView.Filtering._searchbox;

    let onCaretUpdated = ensureCaretAt(panel, 20, 1, true);
    let onThreadPaused = ensureThreadClientState(panel, "paused");
    ContentTask.spawn(tab.linkedBrowser, {}, function* () {
      content.document.querySelector("button").click();
    });
    yield onCaretUpdated;
    yield onThreadPaused

    // Now open a tab and close it.
    let tab2 = yield addTab(TAB_URL);
    yield waitForTick();
    yield removeTab(tab2);
    yield ensureCaretAt(panel, 20);

    // Try to use the Cmd-L keybinding to see if it still works.
    let caretMove = ensureCaretAt(panel, 15, 1, true);
    // Wait a tick for the editor focus event to occur first.
    executeSoon(function () {
      EventUtils.synthesizeKey("l", { accelKey: true });
      EventUtils.synthesizeKey("1", {});
      EventUtils.synthesizeKey("5", {});
    });
    yield caretMove;

    yield resumeDebuggerThenCloseAndFinish(panel);
  }).catch(aError => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  });
}
