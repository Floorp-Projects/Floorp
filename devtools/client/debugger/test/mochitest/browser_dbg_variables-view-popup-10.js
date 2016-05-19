/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Makes sure the source editor's scroll location doesn't change when
 * a variable inspection popup is opened and a watch expression is
 * also evaluated at the same time.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let editor = win.DebuggerView.editor;
    let editorContainer = win.document.getElementById("editor");
    let bubble = win.DebuggerView.VariableBubble;
    let expressions = win.DebuggerView.WatchExpressions;
    let tooltip = bubble._tooltip.panel;

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    let expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    expressions.addExpression("this");
    editor.focus();
    yield expressionsEvaluated;

    // Scroll to the top of the editor and inspect variables.
    let breakpointScrollPosition = editor.getScrollInfo().top;
    editor.setFirstVisibleLine(0);
    let topmostScrollPosition = editor.getScrollInfo().top;

    ok(topmostScrollPosition < breakpointScrollPosition,
      "The editor is now scrolled to the top (0).");
    is(editor.getFirstVisibleLine(), 0,
      "The editor is now scrolled to the top (1).");

    let failPopup = () => ok(false, "The popup has got unexpectedly hidden.");
    let failScroll = () => ok(false, "The editor has got unexpectedly scrolled.");
    tooltip.addEventListener("popuphiding", failPopup);
    editorContainer.addEventListener("scroll", failScroll);
    editor.on("scroll", () => {
      if (editor.getScrollInfo().top > topmostScrollPosition) {
        ok(false, "The editor scrolled back to the breakpoint location.");
      }
    });

    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    yield openVarPopup(panel, { line: 14, ch: 15 });
    yield expressionsEvaluated;

    tooltip.removeEventListener("popuphiding", failPopup);
    editorContainer.removeEventListener("scroll", failScroll);

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
