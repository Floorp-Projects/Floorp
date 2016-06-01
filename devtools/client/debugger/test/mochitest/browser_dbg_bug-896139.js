/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 896139 - Breakpoints not triggering when reloading script.
 */

const TAB_URL = EXAMPLE_URL + "doc_bug-896139.html";
const SCRIPT_URL = "code_bug-896139.js";

function test() {
  Task.spawn(function* () {
    function testBreakpoint() {
      let promise = waitForDebuggerEvents(panel, win.EVENTS.FETCHED_SCOPES);
      callInTab(tab, "f");
      return promise.then(() => doResume(panel));
    }

    let [tab,, panel] = yield initDebugger();
    let win = panel.panelWin;

    let Sources = win.DebuggerView.Sources;

    // Load the debugger against a blank document and load the test url only
    // here and not via initDebugger. That, because this test load SCRIPT_URL
    // dynamically, on load, and the debugger may be on TAB_URL or SCRIPT_URL
    // depending on cpu speed. initDebugger expect to assert one precise
    // source.
    yield navigateActiveTabTo(panel,
                              TAB_URL,
                              win.EVENTS.SOURCE_SHOWN);

    if (Sources.selectedItem.attachment.source.url.indexOf(SCRIPT_URL) === -1) {
      // If there is only the html file, wait for the js file to be listed.
      if (Sources.itemCount == 1) {
        yield waitForDebuggerEvents(panel, win.EVENTS.NEW_SOURCE);
        // Wait for it to be added to the UI
        yield waitForTick();
      }
      // Select the js file.
      let onSource = waitForSourceAndCaret(panel, SCRIPT_URL, 1);
      Sources.selectedValue = getSourceActor(win.DebuggerView.Sources,
                                             EXAMPLE_URL + SCRIPT_URL);
      yield onSource;
    }

    yield panel.addBreakpoint({
      actor: getSourceActor(win.DebuggerView.Sources, EXAMPLE_URL + SCRIPT_URL),
      line: 6
    });

    // Race condition: the setBreakpoint request sometimes leaves the
    // debugger in paused state for a bit because we are called before
    // that request finishes (see bug 1156531 for plans to fix)
    if (panel.panelWin.gThreadClient.state !== "attached") {
      yield waitForThreadEvents(panel, "resumed");
    }

    yield testBreakpoint();
    yield reloadActiveTab(panel, win.EVENTS.SOURCE_SHOWN);
    yield testBreakpoint();

    yield closeDebuggerAndFinish(panel);
  });
}
