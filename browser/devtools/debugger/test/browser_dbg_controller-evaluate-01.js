/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the public evaluation API from the debugger controller.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let frames = win.DebuggerController.StackFrames;
    let framesView = win.DebuggerView.StackFrames;
    let sources = win.DebuggerController.SourceScripts;
    let sourcesView = win.DebuggerView.Sources;
    let editorView = win.DebuggerView.editor;
    let events = win.EVENTS;

    function checkView(frameDepth, selectedSource, caretLine, editorText) {
      is(win.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      is(framesView.itemCount, 4,
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

    // Cache the sources text to avoid having to wait for their retrieval.
    yield promise.all(sourcesView.attachments.map(e => sources.getText(e.source)));
    is(sources._cache.size, 2, "There should be two cached sources in the cache.");

    // Eval while not paused.
    try {
      yield frames.evaluate("foo");
    } catch (error) {
      is(error.message, "No stack frame available.",
        "Evaluating shouldn't work while the debuggee isn't paused.");
    }

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.firstCall());
    yield waitForSourceAndCaretAndScopes(panel, "-02.js", 1);
    checkView(0, 1, 1, [/secondCall/, 118]);

    // Eval in the topmost frame, while paused.
    let updatedView = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    let result = yield frames.evaluate("foo");
    ok(!result.throw, "The evaluation hasn't thrown.");
    is(result.return.type, "object", "The evaluation return type is correct.");
    is(result.return.class, "Function", "The evaluation return class is correct.");

    yield updatedView;
    checkView(0, 1, 1, [/secondCall/, 118]);
    ok(true, "Evaluating in the topmost frame works properly.");

    // Eval in a different frame, while paused.
    let updatedView = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    try {
      yield frames.evaluate("foo", { depth: 3 }); // oldest frame
    } catch (result) {
      is(result.return.type, "object", "The evaluation thrown type is correct.");
      is(result.return.class, "Error", "The evaluation thrown class is correct.");
      ok(!result.return, "The evaluation hasn't returned.");
    }

    yield updatedView;
    checkView(0, 1, 1, [/secondCall/, 118]);
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
