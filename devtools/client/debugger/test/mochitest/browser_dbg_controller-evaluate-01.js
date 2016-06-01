/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the public evaluation API from the debugger controller.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  Task.spawn(function* () {
    const options = {
      source: EXAMPLE_URL + "code_script-switching-01.js",
      line: 1
    };
    const [tab,, panel] = yield initDebugger(TAB_URL, options);
    const win = panel.panelWin;
    const frames = win.DebuggerController.StackFrames;
    const framesView = win.DebuggerView.StackFrames;
    const sourcesView = win.DebuggerView.Sources;
    const editorView = win.DebuggerView.editor;
    const events = win.EVENTS;
    const queries = win.require("./content/queries");
    const constants = win.require("./content/constants");
    const actions = bindActionCreators(panel);
    const getState = win.DebuggerController.getState;

    function checkView(frameDepth, selectedSource, caretLine, editorText) {
      is(win.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      is(framesView.itemCount, 2,
        "Should have four frames.");
      is(framesView.selectedDepth, frameDepth,
        "The correct frame is selected in the widget.");
      is(sourcesView.selectedIndex, selectedSource,
        "The correct source is selected in the widget.");
      ok(isCaretPos(panel, caretLine),
        "Editor caret location is correct.");
      is(editorView.getText().search(editorText[0]), editorText[1],
        "The correct source is not displayed.");
    }

    // Cache the sources text to avoid having to wait for their
    // retrieval.
    const sources = queries.getSources(getState());
    yield promise.all(Object.keys(sources).map(k => {
      return actions.loadSourceText(sources[k]);
    }));

    is(Object.keys(getState().sources.sourcesText).length, 2,
       "There should be two cached sources in the cache.");

    // Eval while not paused.
    try {
      yield frames.evaluate("foo");
    } catch (error) {
      is(error.message, "No stack frame available.",
        "Evaluating shouldn't work while the debuggee isn't paused.");
    }

    callInTab(tab, "firstCall");
    yield waitForSourceAndCaretAndScopes(panel, "-02.js", 6);
    checkView(0, 1, 6, [/secondCall/, 118]);

    // Eval in the topmost frame, while paused.
    let updatedView = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    let result = yield frames.evaluate("foo");
    ok(!result.throw, "The evaluation hasn't thrown.");
    is(result.return.type, "object", "The evaluation return type is correct.");
    is(result.return.class, "Function", "The evaluation return class is correct.");

    yield updatedView;
    checkView(0, 1, 6, [/secondCall/, 118]);
    ok(true, "Evaluating in the topmost frame works properly.");

    // Eval in a different frame, while paused.
    updatedView = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    try {
      yield frames.evaluate("foo", { depth: 1 }); // oldest frame
    } catch (result) {
      is(result.return.type, "object", "The evaluation thrown type is correct.");
      is(result.return.class, "Error", "The evaluation thrown class is correct.");
      ok(!result.return, "The evaluation hasn't returned.");
    }

    yield updatedView;
    checkView(0, 1, 6, [/secondCall/, 118]);
    ok(true, "Evaluating in a custom frame works properly.");

    // Eval in a non-existent frame, while paused.
    waitForDebuggerEvents(panel, events.FETCHED_SCOPES).then(() => {
      ok(false, "Shouldn't have updated the view when trying to evaluate " +
        "an expression in a non-existent stack frame.");
    });
    try {
      yield frames.evaluate("foo", { depth: 4 }); // non-existent frame
    } catch (error) {
      is(error.message, "No stack frame available.",
        "Evaluating shouldn't work if the specified frame doesn't exist.");
    }

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
