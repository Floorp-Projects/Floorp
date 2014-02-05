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

    function checkView(selectedFrame, selectedSource, caretLine, editorText) {
      is(win.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      is(framesView.itemCount, 4,
        "Should have four frames.");
      is(framesView.selectedDepth, selectedFrame,
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

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.firstCall());
    yield waitForSourceAndCaretAndScopes(panel, "-02.js", 1);
    checkView(0, 1, 1, [/secondCall/, 118]);

    // Change the selected frame and eval inside it.
    let updatedFrame = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    framesView.selectedDepth = 3; // oldest frame
    yield updatedFrame;
    checkView(3, 0, 5, [/firstCall/, 118]);

    let updatedView = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    try {
      yield frames.evaluate("foo");
    } catch (result) {
      is(result.return.type, "object", "The evaluation thrown type is correct.");
      is(result.return.class, "Error", "The evaluation thrown class is correct.");
      ok(!result.return, "The evaluation hasn't returned.");
    }

    yield updatedView;
    checkView(3, 0, 5, [/firstCall/, 118]);
    ok(true, "Evaluating while in a user-selected frame works properly.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
