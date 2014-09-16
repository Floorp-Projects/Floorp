/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that keybindings still work when the content window is paused and
// the tab is selected again.

function test() {
  Task.spawn(function* () {
    const TAB_URL = EXAMPLE_URL + "doc_inline-script.html";
    let gDebugger, searchBox;

    let [, debuggee, panel] = yield initDebugger(TAB_URL);
    gDebugger = panel.panelWin;
    searchBox = gDebugger.DebuggerView.Filtering._searchbox;

    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" },
        debuggee.document.querySelector("button"),
        debuggee);
    });
    yield waitForSourceAndCaretAndScopes(panel, ".html", 20);
    yield ensureThreadClientState(panel, "paused");

    // Now open a tab and close it.
    let tab2 = yield addTab(TAB_URL);
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
  }).then(null, aError => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  });
}
