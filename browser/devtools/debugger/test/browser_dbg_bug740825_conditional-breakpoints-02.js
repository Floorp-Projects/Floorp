/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 740825: test the debugger conditional breakpoints.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_conditional-breakpoints.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gEditor = null;
let gSources = null;
let gBreakpoints = null;

requestLongerTimeout(2);

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

    gDebugger.addEventListener("Debugger:SourceShown", onScriptShown);

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });
    resumed = true;

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function() {
      gDebuggee.ermahgerd(); // ermahgerd!!
    });
  });

  function onScriptShown(aEvent)
  {
    scriptShown = aEvent.detail.url.indexOf("conditional-breakpoints") != -1;
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
    gEditor = gDebugger.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gPane.getAllBreakpoints();

    is(gDebugger.DebuggerController.activeThread.state, "paused",
      "Should only be getting stack frames while paused.");

    is(gSources.itemCount, 1,
      "Found the expected number of scripts.");

    isnot(gEditor.getText().indexOf("ermahgerd"), -1,
      "The correct script was loaded initially.");

    is(gSources.selectedValue, gSources.values[0],
      "The correct script is selected");

    is(Object.keys(gBreakpoints), 0, "no breakpoints");
    ok(!gPane.getBreakpoint("foo", 3), "getBreakpoint('foo', 3) returns falsey");
    is(gEditor.getBreakpoints().length, 0, "no breakpoints in the editor");

    executeSoon(addBreakpoint1);
  }

  function addBreakpoint1()
  {
    gPane.addBreakpoint({ url: gSources.selectedValue, line: 12 });

    waitForBreakpoint(12, function() {
      waitForCaretPos(10, function() {
        waitForPopup(false, function() {
          testBreakpoint(gSources.selectedBreakpointItem,
                         gSources.selectedBreakpointClient,
                         gSources.selectedValue, 12, false, false, false);

          executeSoon(addBreakpoint2);
        });
      });
    });
  }

  function addBreakpoint2()
  {
    gSources._editorContextMenuLineNumber = 12;
    gSources._onCmdAddBreakpoint();

    waitForBreakpoint(13, function() {
      waitForCaretPos(12, function() {
        waitForPopup(false, function() {
          testBreakpoint(gSources.selectedBreakpointItem,
                         gSources.selectedBreakpointClient,
                         gSources.selectedValue, 13, false, false, true);

          executeSoon(modBreakpoint2);
        });
      });
    });
  }

  function modBreakpoint2()
  {
    gSources._editorContextMenuLineNumber = 12;
    gSources._onCmdAddConditionalBreakpoint();

    waitForBreakpoint(13, function() {
      waitForCaretPos(12, function() {
        waitForPopup(true, function() {
          testBreakpoint(gSources.selectedBreakpointItem,
                         gSources.selectedBreakpointClient,
                         gSources.selectedValue, 13, true, true, true);

          executeSoon(addBreakpoint3);
        });
      });
    });
  }

  function addBreakpoint3()
  {
    gSources._editorContextMenuLineNumber = 13;
    gSources._onCmdAddConditionalBreakpoint();

    waitForBreakpoint(14, function() {
      waitForCaretPos(13, function() {
        waitForPopup(true, function() {
          testBreakpoint(gSources.selectedBreakpointItem,
                         gSources.selectedBreakpointClient,
                         gSources.selectedValue, 14, true, true, true);

          executeSoon(modBreakpoint3);
        });
      });
    });
  }

  function modBreakpoint3()
  {
    write("bamboocha");
    EventUtils.sendKey("RETURN", gDebugger);

    waitForBreakpoint(14, function() {
      waitForCaretPos(13, function() {
        waitForPopup(false, function() {
          is(gSources.selectedBreakpointClient.conditionalExpression, "bamboocha",
            "The bamboocha expression wasn't fonud on the conditional breakpoint");

          executeSoon(setContextMenu);
        });
      });
    });
  }

  function setContextMenu()
  {
    let contextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");
    info("Testing source editor popup...");

    contextMenu.addEventListener("popupshown", function onPopupShown() {
      contextMenu.removeEventListener("popupshown", onPopupShown, false);
      info("Source editor popup shown...");

      contextMenu.addEventListener("popuphidden", function onPopupHidden() {
        contextMenu.removeEventListener("popuphidden", onPopupHidden, false);
        info("Source editor popup hidden...");

        is(gSources._editorContextMenuLineNumber, 14,
          "The context menu line number is incorrect after the popup was hidden.");
        executeSoon(addBreakpoint4);
      }, false);

      is(gSources._editorContextMenuLineNumber, 14,
        "The context menu line number is incorrect after the popup was shown.");
      contextMenu.hidePopup();
    }, false);

    is(gSources._editorContextMenuLineNumber, -1,
      "The context menu line number was incorrect before the popup was shown.");
    gSources._editorContextMenuLineNumber = 14;
    contextMenu.openPopup(gEditor.editorElement, "overlap", 0, 0, true, false);
  }

  function addBreakpoint4()
  {
    gEditor.setCaretPosition(14);
    gSources._onCmdAddBreakpoint();

    waitForBreakpoint(15, function() {
      waitForCaretPos(14, function() {
        waitForPopup(false, function() {
          testBreakpoint(gSources.selectedBreakpointItem,
                         gSources.selectedBreakpointClient,
                         gSources.selectedValue, 15, false, false, true);

          executeSoon(delBreakpoint4);
        });
      });
    });
  }

  function delBreakpoint4()
  {
    gEditor.setCaretPosition(14);
    gSources._onCmdAddBreakpoint();

    waitForBreakpoint(null, function() {
      waitForCaretPos(14, function() {
        waitForPopup(false, function() {
          is(gSources.selectedBreakpointItem, null,
            "There should be no selected breakpoint in the breakpoints pane.")
          is(gSources._conditionalPopupVisible, false,
            "The breakpoint conditional expression popup should not be shown.");

          executeSoon(moveHighlight1);
        });
      });
    });
  }

  function moveHighlight1()
  {
    gEditor.setCaretPosition(13);

    waitForBreakpoint(14, function() {
      waitForCaretPos(13, function() {
        waitForPopup(false, function() {
          testBreakpoint(gSources.selectedBreakpointItem,
                         gSources.selectedBreakpointClient,
                         gSources.selectedValue, 14, false, true, true);

          executeSoon(testHighlights1);
        });
      });
    });
  }

  function testHighlights1()
  {
    isnot(gSources.selectedBreakpointItem, null,
      "There should be a selected breakpoint in the breakpoints pane.");
    is(gSources.selectedBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
      "The selected breakpoint should have the correct location.");
    is(gSources.selectedBreakpointItem.attachment.lineNumber, 14,
      "The selected breakpoint should have the correct line number.");
    is(gSources._conditionalPopupVisible, false,
      "The breakpoint conditional expression popup should not be shown.");
    is(gEditor.getCaretPosition().line, 13,
      "The source editor caret position should be at line 13");
    is(gEditor.getCaretPosition().col, 0,
      "The source editor caret position should be at column 0");

    gEditor.setCaretPosition(12);

    waitForCaretPos(12, function() {
      waitForPopup(false, function() {
        isnot(gSources.selectedBreakpointItem, null,
          "There should be a selected breakpoint in the breakpoints pane.");
        is(gSources.selectedBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
          "The selected breakpoint should have the correct location.");
        is(gSources.selectedBreakpointItem.attachment.lineNumber, 13,
          "The selected breakpoint should have the correct line number.");
        is(gSources._conditionalPopupVisible, false,
          "The breakpoint conditional expression popup should not be shown.");
        is(gEditor.getCaretPosition().line, 12,
          "The source editor caret position should be at line 12");
        is(gEditor.getCaretPosition().col, 0,
          "The source editor caret position should be at column 0");

        gEditor.setCaretPosition(11);

        waitForCaretPos(11, function() {
          waitForPopup(false, function() {
            isnot(gSources.selectedBreakpointItem, null,
              "There should be a selected breakpoint in the breakpoints pane.");
            is(gSources.selectedBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
              "The selected breakpoint should have the correct location.");
            is(gSources.selectedBreakpointItem.attachment.lineNumber, 12,
              "The selected breakpoint should have the correct line number.");
            is(gSources._conditionalPopupVisible, false,
              "The breakpoint conditional expression popup should not be shown.");
            is(gEditor.getCaretPosition().line, 11,
              "The source editor caret position should be at line 11");
            is(gEditor.getCaretPosition().col, 0,
              "The source editor caret position should be at column 0");

            gEditor.setCaretPosition(10);

            waitForCaretPos(10, function() {
              waitForPopup(false, function() {
                is(gSources.selectedBreakpointItem, null,
                  "There should not be a selected breakpoint in the breakpoints pane.");
                is(gSources._conditionalPopupVisible, false,
                  "The breakpoint conditional expression popup should not be shown.");
                is(gEditor.getCaretPosition().line, 10,
                  "The source editor caret position should be at line 10");
                is(gEditor.getCaretPosition().col, 0,
                  "The source editor caret position should be at column 0");

                gEditor.setCaretPosition(14);

                waitForCaretPos(14, function() {
                  waitForPopup(false, function() {
                    is(gSources.selectedBreakpointItem, null,
                      "There should not be a selected breakpoint in the breakpoints pane.");
                    is(gSources._conditionalPopupVisible, false,
                      "The breakpoint conditional expression popup should not be shown.");
                    is(gEditor.getCaretPosition().line, 14,
                      "The source editor caret position should be at line 14");
                    is(gEditor.getCaretPosition().col, 0,
                      "The source editor caret position should be at column 0");

                    executeSoon(testHighlights2);
                  });
                });
              });
            });
          });
        });
      });
    });
  }

  function testHighlights2()
  {
    EventUtils.sendMouseEvent({ type: "click" },
      gSources.widget._list.querySelectorAll(".dbg-breakpoint")[2],
      gDebugger);

    waitForCaretPos(13, function() {
      waitForPopup(true, function() {
        isnot(gSources.selectedBreakpointItem, null,
          "There should be a selected breakpoint in the breakpoints pane.");
        is(gSources.selectedBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
          "The selected breakpoint should have the correct location.");
        is(gSources.selectedBreakpointItem.attachment.lineNumber, 14,
          "The selected breakpoint should have the correct line number.");
        is(gSources._conditionalPopupVisible, true,
          "The breakpoint conditional expression popup should be shown.");
        is(gEditor.getCaretPosition().line, 13,
          "The source editor caret position should be at line 13");
        is(gEditor.getCaretPosition().col, 0,
          "The source editor caret position should be at column 0");

        EventUtils.sendMouseEvent({ type: "click" },
          gSources.widget._list.querySelectorAll(".dbg-breakpoint")[1],
          gDebugger);

        waitForCaretPos(12, function() {
          waitForPopup(true, function() {
            isnot(gSources.selectedBreakpointItem, null,
              "There should be a selected breakpoint in the breakpoints pane.");
            is(gSources.selectedBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
              "The selected breakpoint should have the correct location.");
            is(gSources.selectedBreakpointItem.attachment.lineNumber, 13,
              "The selected breakpoint should have the correct line number.");
            is(gSources._conditionalPopupVisible, true,
              "The breakpoint conditional expression popup should be shown.");
            is(gEditor.getCaretPosition().line, 12,
              "The source editor caret position should be at line 12");
            is(gEditor.getCaretPosition().col, 0,
              "The source editor caret position should be at column 0");

            EventUtils.sendMouseEvent({ type: "click" },
              gSources.widget._list.querySelectorAll(".dbg-breakpoint")[0],
              gDebugger);

            waitForCaretPos(11, function() {
              waitForPopup(false, function() {
                isnot(gSources.selectedBreakpointItem, null,
                  "There should be a selected breakpoint in the breakpoints pane.");
                is(gSources.selectedBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
                  "The selected breakpoint should have the correct location.");
                is(gSources.selectedBreakpointItem.attachment.lineNumber, 12,
                  "The selected breakpoint should have the correct line number.");
                is(gSources._conditionalPopupVisible, false,
                  "The breakpoint conditional expression popup should be shown.");
                is(gEditor.getCaretPosition().line, 11,
                  "The source editor caret position should be at line 11");
                is(gEditor.getCaretPosition().col, 0,
                  "The source editor caret position should be at column 0");

                executeSoon(delBreakpoint2);
              });
            });
          });
        });
      });
    });
  }

  function delBreakpoint2()
  {
    gSources._editorContextMenuLineNumber = 12;
    gSources._onCmdAddBreakpoint();

    waitForBreakpoint(null, function() {
      waitForPopup(false, function() {
        is(gSources.selectedBreakpointItem, null,
          "There should be no selected breakpoint in the breakpoints pane.")
        is(gSources._conditionalPopupVisible, false,
          "The breakpoint conditional expression popup should not be shown.");

        executeSoon(delBreakpoint3);
      });
    });
  }

  function delBreakpoint3()
  {
    gSources._editorContextMenuLineNumber = 13;
    gSources._onCmdAddBreakpoint();

    waitForBreakpoint(null, function() {
      waitForPopup(false, function() {
        is(gSources.selectedBreakpointItem, null,
          "There should be no selected breakpoint in the breakpoints pane.")
        is(gSources._conditionalPopupVisible, false,
          "The breakpoint conditional expression popup should not be shown.");

        executeSoon(testBreakpoints);
      });
    });
  }

  function testBreakpoints()
  {
    is(Object.keys(gBreakpoints).length, 1, "one breakpoint");
    ok(!gPane.getBreakpoint("foo", 3), "getBreakpoint('foo', 3) returns falsey");
    is(gEditor.getBreakpoints().length, 1, "one breakpoint in the editor");

    closeDebuggerAndFinish();
  }

  function testBreakpoint(aBreakpointItem, aBreakpointClient, url, line, popup, conditional, editor)
  {
    is(aBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
      "The breakpoint on line " + line + " wasn't added on the correct source.");
    is(aBreakpointItem.attachment.lineNumber, line,
      "The breakpoint on line " + line + " wasn't found.");
    is(!aBreakpointItem.attachment.disabled, true,
      "The breakpoint on line " + line + " should be enabled.");
    is(gSources._conditionalPopupVisible, popup,
      "The breakpoint conditional expression popup should " + (popup ? "" : "not ") + "be shown.");

    is(aBreakpointClient.location.url, url,
       "The breakpoint's client url is correct");
    is(aBreakpointClient.location.line, line,
       "The breakpoint's client line is correct");

    if (conditional) {
      isnot(aBreakpointClient.conditionalExpression, undefined,
        "The breakpoint on line " + line + " should have a conditional expression.");
    } else {
      is(aBreakpointClient.conditionalExpression, undefined,
        "The breakpoint on line " + line + " should not have a conditional expression.");
    }

    if (editor) {
      is(gEditor.getCaretPosition().line + 1, line,
        "The editor caret position is not situated on the proper line.");
      is(gEditor.getCaretPosition().col, 0,
        "The editor caret position is not situated on the proper column.");
    }
  }

  let bogusClient = {
    location: {
      url: null,
      line: null
    }
  };

  function waitForBreakpoint(expected, callback) {
    // Poll every few milliseconds until expected breakpoint is hit.
    let count = 0;
    let intervalID = window.setInterval(function() {
      info("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the breakpoint.");
        window.clearInterval(intervalID);
        return closeDebuggerAndFinish();
      }
      if ((gSources.selectedBreakpointClient !== expected) &&
          (gSources.selectedBreakpointClient || bogusClient).location.line !== expected) {
        return;
      }
      // We arrived at the expected line, it's safe to callback.
      window.clearInterval(intervalID);
      callback();
    }, 100);
  }

  function waitForCaretPos(number, callback)
  {
    // Poll every few milliseconds until the source editor line is active.
    let count = 0;
    let intervalID = window.setInterval(function() {
      info("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the line.");
        window.clearInterval(intervalID);
        return closeDebuggerAndFinish();
      }
      if (gEditor.getCaretPosition().line != number) {
        return;
      }
      // We got the source editor at the expected line, it's safe to callback.
      window.clearInterval(intervalID);
      callback();
    }, 100);
  }

  function waitForPopup(state, callback)
  {
    // Poll every few milliseconds until the expression popup is shown.
    let count = 0;
    let intervalID = window.setInterval(function() {
      info("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the popup.");
        window.clearInterval(intervalID);
        return closeDebuggerAndFinish();
      }
      if (gSources._conditionalPopupVisible != state) {
        return;
      }
      // We got the expression popup at the expected state, it's safe to callback.
      window.clearInterval(intervalID);
      callback();
    }, 100);
  }

  function clear() {
    gSources._cbTextbox.focus();
    gSources._cbTextbox.value = "";
  }

  function write(text) {
    clear();
    append(text);
  }

  function append(text) {
    gSources._cbTextbox.focus();

    for (let i = 0; i < text.length; i++) {
      EventUtils.sendChar(text[i], gDebugger);
    }
  }

  registerCleanupFunction(function() {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gEditor = null;
    gSources = null;
    gBreakpoints = null;
  });
}
