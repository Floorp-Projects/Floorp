/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let resumed = false;
  let testStarted = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    resumed = true;

    gDebugger.addEventListener("Debugger:SourceShown", onScriptShown);

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function() {
      gDebuggee.firstCall();
    });
  });

  function onScriptShown(aEvent)
  {
    scriptShown = aEvent.detail.url.indexOf("-02.js") != -1;
    executeSoon(startTest);
  }

  function startTest()
  {
    if (scriptShown && framesAdded && resumed && !testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function performTest()
  {
    let editor = gDebugger.DebuggerView.editor;
    let sources = gDebugger.DebuggerView.Sources;
    let stackframes = gDebugger.DebuggerView.StackFrames;

    is(editor.getCaretPosition().line, 5,
      "The source editor caret position was incorrect (1).");
    is(sources.selectedLabel, "test-script-switching-02.js",
      "The currently selected source is incorrect (1).");
    is(stackframes.selectedIndex, 3,
      "The currently selected stackframe is incorrect (1).");

    EventUtils.sendKey("DOWN", gDebugger);
    is(editor.getCaretPosition().line, 6,
      "The source editor caret position was incorrect (2).");
    is(sources.selectedLabel, "test-script-switching-02.js",
      "The currently selected source is incorrect (2).");
    is(stackframes.selectedIndex, 3,
      "The currently selected stackframe is incorrect (2).");

    EventUtils.sendKey("UP", gDebugger);
    is(editor.getCaretPosition().line, 5,
      "The source editor caret position was incorrect (3).");
    is(sources.selectedLabel, "test-script-switching-02.js",
      "The currently selected source is incorrect (3).");
    is(stackframes.selectedIndex, 3,
      "The currently selected stackframe is incorrect (3).");


    EventUtils.sendMouseEvent({ type: "mousedown" },
      stackframes.selectedItem.target,
      gDebugger);


    EventUtils.sendKey("UP", gDebugger);
    is(editor.getCaretPosition().line, 5,
      "The source editor caret position was incorrect (4).");
    is(sources.selectedLabel, "test-script-switching-02.js",
      "The currently selected source is incorrect (4).");
    is(stackframes.selectedIndex, 2,
      "The currently selected stackframe is incorrect (4).");

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      is(editor.getCaretPosition().line, 4,
        "The source editor caret position was incorrect (5).");
      is(sources.selectedLabel, "test-script-switching-01.js",
        "The currently selected source is incorrect (5).");
      is(stackframes.selectedIndex, 1,
        "The currently selected stackframe is incorrect (5).");

      EventUtils.sendKey("UP", gDebugger);

      is(editor.getCaretPosition().line, 4,
        "The source editor caret position was incorrect (6).");
      is(sources.selectedLabel, "test-script-switching-01.js",
        "The currently selected source is incorrect (6).");
      is(stackframes.selectedIndex, 0,
        "The currently selected stackframe is incorrect (6).");

      gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);

        is(editor.getCaretPosition().line, 5,
          "The source editor caret position was incorrect (7).");
        is(sources.selectedLabel, "test-script-switching-02.js",
          "The currently selected source is incorrect (7).");
        is(stackframes.selectedIndex, 3,
          "The currently selected stackframe is incorrect (7).");

        gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
          gDebugger.removeEventListener(aEvent.type, _onEvent);

          is(editor.getCaretPosition().line, 4,
            "The source editor caret position was incorrect (8).");
          is(sources.selectedLabel, "test-script-switching-01.js",
            "The currently selected source is incorrect (8).");
          is(stackframes.selectedIndex, 0,
            "The currently selected stackframe is incorrect (8).");

          closeDebuggerAndFinish();
        });

        EventUtils.sendKey("HOME", gDebugger);
      });

      EventUtils.sendKey("END", gDebugger);
    });

    EventUtils.sendKey("UP", gDebugger);
  }

  registerCleanupFunction(function() {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
  });
}
