/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view is able to override getter properties
 * to plain value properties.
 */

function test() {
  const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  Task.spawn(function* () {
    let [, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let L10N = win.L10N;
    let editor = win.DebuggerView.editor;
    let vars = win.DebuggerView.Variables;
    let watch = win.DebuggerView.WatchExpressions;

    vars.switch = function() {};
    vars.delete = function() {};

    let paused = waitForSourceAndCaretAndScopes(panel, ".html", 24);
    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" },
        debuggee.document.querySelector("button"),
        debuggee);
    });
    yield paused;

    // Add a watch expression for direct observation of the value change.
    let addedWatch = waitForDebuggerEvents(panel, win.EVENTS.FETCHED_WATCH_EXPRESSIONS);
    watch.addExpression("myVar.prop");
    editor.focus();
    yield addedWatch;

    // Scroll myVar into view.
    vars.focusLastVisibleItem();

    let localScope = vars.getScopeAtIndex(1);
    let myVar = localScope.get("myVar");

    myVar.expand();
    vars.clearHierarchy();

    yield waitForDebuggerEvents(panel, win.EVENTS.FETCHED_PROPERTIES);

    let editTarget = myVar.get("prop").target;

    // Allow the target variable to get painted, so that clicking on
    // its value would scroll the new textbox node into view.
    executeSoon(() => {
      let varEdit = editTarget.querySelector(".title > .variables-view-edit");
      EventUtils.sendMouseEvent({ type: "mousedown" }, varEdit, win);

      let varInput = editTarget.querySelector(".title > .element-value-input");
      setText(varInput, "\"xlerb\"");
      EventUtils.sendKey("RETURN", win);
    });

    yield waitForDebuggerEvents(panel, win.EVENTS.FETCHED_WATCH_EXPRESSIONS);

    let exprScope = vars.getScopeAtIndex(0);

    ok(exprScope,
      "There should be a wach expressions scope in the variables view.");
    is(exprScope.name, L10N.getStr("watchExpressionsScopeLabel"),
      "The scope's name should be marked as 'Watch Expressions'.");
    is(exprScope._store.size, 1,
      "There should be one evaluation available.");

    is(exprScope.get("myVar.prop").value, "xlerb",
      "The expression value is correct after the edit.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  }).then(null, aError => {
      ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  });
}
