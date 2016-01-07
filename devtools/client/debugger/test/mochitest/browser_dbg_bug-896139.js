/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 896139 - Breakpoints not triggering when reloading script.
 */

const TAB_URL = "doc_bug-896139.html";
const SCRIPT_URL = "code_bug-896139.js";

function test() {
  Task.spawn(function* () {
    function testBreakpoint() {
      let promise = waitForDebuggerEvents(panel, win.EVENTS.FETCHED_SCOPES);
      callInTab(tab, "f");
      return promise.then(() => doResume(panel));
    }

    let [tab,, panel] = yield initDebugger(EXAMPLE_URL + TAB_URL);
    let win = panel.panelWin;

    let Sources = win.DebuggerView.Sources;
    yield waitForDebuggerEvents(panel, win.EVENTS.SOURCE_SHOWN);
    if (Sources.selectedItem.attachment.source.url.indexOf(SCRIPT_URL) === -1) {
      Sources.selectedValue = getSourceActor(win.DebuggerView.Sources, EXAMPLE_URL + SCRIPT_URL)
    }

    yield panel.addBreakpoint({
      actor: getSourceActor(win.DebuggerView.Sources, EXAMPLE_URL + SCRIPT_URL),
      line: 6
    });

    // Race condition: the setBreakpoint request sometimes leaves the
    // debugger in paused state for a bit because we are called before
    // that request finishes (see bug 1156531 for plans to fix)
    if(panel.panelWin.gThreadClient.state !== "attached") {
      yield waitForThreadEvents(panel, "resumed");
    }

    yield testBreakpoint();
    yield reloadActiveTab(panel, win.EVENTS.SOURCE_SHOWN);
    yield testBreakpoint();

    yield closeDebuggerAndFinish(panel);
  });
}
