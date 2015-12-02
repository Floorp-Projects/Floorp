/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the public evaluation API from the debugger controller.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  Task.spawn(function*() {
    const [tab,, panel] = yield initDebugger(TAB_URL);
    const win = panel.panelWin;
    const frames = win.DebuggerController.StackFrames;
    const framesView = win.DebuggerView.StackFrames;
    const sourcesView = win.DebuggerView.Sources;
    const editorView = win.DebuggerView.editor;
    const events = win.EVENTS;
    const queries = win.require('./content/queries');
    const constants = win.require('./content/constants');
    const actions = bindActionCreators(panel);
    const getState = win.DebuggerController.getState;

    function checkView(selectedFrame, selectedSource, caretLine, editorText) {
      is(win.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      is(framesView.itemCount, 2,
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

    // Cache the sources text to avoid having to wait for their
    // retrieval.
    const sources = queries.getSources(getState());
    yield promise.all(Object.keys(sources).map(k => {
      return actions.loadSourceText(sources[k]);
    }));

    // Allow this generator function to yield first.
    callInTab(tab, "firstCall");
    yield waitForSourceAndCaretAndScopes(panel, "-02.js", 6);
    checkView(0, 1, 6, [/secondCall/, 118]);

    // Change the selected frame and eval inside it.
    let updatedFrame = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    framesView.selectedDepth = 1; // oldest frame
    yield updatedFrame;
    checkView(1, 0, 5, [/firstCall/, 118]);

    let updatedView = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    try {
      yield frames.evaluate("foo");
    } catch (result) {
      is(result.return.type, "object", "The evaluation thrown type is correct.");
      is(result.return.class, "Error", "The evaluation thrown class is correct.");
      ok(!result.return, "The evaluation hasn't returned.");
    }

    yield updatedView;
    checkView(1, 0, 5, [/firstCall/, 118]);
    ok(true, "Evaluating while in a user-selected frame works properly.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
