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

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });
    resumed = true;

    gDebugger.addEventListener("Debugger:SourceShown", onScriptShown);

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
      Services.tm.currentThread.dispatch({ run: addBreakpoints }, 0);
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

    is(Object.keys(gBreakpoints).length, 13, "thirteen breakpoints");
    ok(!gPane.getBreakpoint("foo", 3), "getBreakpoint('foo', 3) returns falsey");
    is(gEditor.getBreakpoints().length, 13, "thirteen breakpoints in the editor");

    executeSoon(test1);
  }

  function test1(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 14, test2);
  }

  function test2(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 15, test3);
  }

  function test3(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 16, test4);
  }

  function test4(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 17, test5);
  }

  function test5(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 18, test6);
  }

  function test6(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 19, test7);
  }

  function test7(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 21, test8);
  }

  function test8(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 22, test9);
  }

  function test9(callback)
  {
    resumeAndTestBreakpoint(gSources.selectedValue, 23, test10);
  }

  function test10(callback)
  {
    gDebugger.addEventListener("Debugger:AfterFramesCleared", function listener() {
      gDebugger.removeEventListener("Debugger:AfterFramesCleared", listener, true);

      isnot(gSources.selectedItem, null,
        "There should be a selected script in the scripts pane.")
      is(gSources.selectedBreakpointItem, null,
        "There should be no selected breakpoint in the scripts pane.")
      is(gSources.selectedBreakpointClient, null,
        "There should be no selected client in the scripts pane.");
      is(gSources._conditionalPopupVisible, false,
        "The breakpoint conditional expression popup should not be shown.");

      is(gDebugger.DebuggerView.StackFrames.widget._list.querySelectorAll(".dbg-stackframe").length, 0,
        "There should be no visible stackframes.");
      is(gDebugger.DebuggerView.Sources.widget._list.querySelectorAll(".dbg-breakpoint").length, 13,
        "There should be thirteen visible breakpoints.");

      testReload();
    }, true);

    gDebugger.DebuggerController.activeThread.resume();
  }

  function resumeAndTestBreakpoint(url, line, callback)
  {
    resume(line, function() {
      waitForCaretPos(line - 1, function() {
        testBreakpoint(gSources.selectedBreakpointItem, gSources.selectedBreakpointClient, url, line, true);
        callback();
      });
    });
  }

  function testBreakpoint(aBreakpointItem, aBreakpointClient, url, line, editor)
  {
    is(aBreakpointItem.attachment.sourceLocation, gSources.selectedValue,
      "The breakpoint on line " + line + " wasn't added on the correct source.");
    is(aBreakpointItem.attachment.lineNumber, line,
      "The breakpoint on line " + line + " wasn't found.");
    is(!!aBreakpointItem.attachment.disabled, false,
      "The breakpoint on line " + line + " should be enabled.");
    is(!!aBreakpointItem.attachment.openPopupFlag, false,
      "The breakpoint on line " + line + " should not open a popup.");
    is(gSources._conditionalPopupVisible, false,
      "The breakpoint conditional expression popup should not be shown.");

    is(aBreakpointClient.location.url, url,
       "The breakpoint's client url is correct");
    is(aBreakpointClient.location.line, line,
       "The breakpoint's client line is correct");
    isnot(aBreakpointClient.conditionalExpression, undefined,
      "The breakpoint on line " + line + " should have a conditional expression.");

    if (editor) {
      is(gEditor.getCaretPosition().line + 1, line,
        "The editor caret position is not situated on the proper line.");
      is(gEditor.getCaretPosition().col, 0,
        "The editor caret position is not situated on the proper column.");
    }
  }

  function addBreakpoints(callback)
  {
    let currentUrl = gDebugger.DebuggerView.Sources.selectedValue;

    gPane.addBreakpoint({ url: currentUrl, line: 12 }, function() {
      gPane.addBreakpoint({ url: currentUrl, line: 13 }, function() {
        gPane.addBreakpoint({ url: currentUrl, line: 14 }, function() {
          gPane.addBreakpoint({ url: currentUrl, line: 15 }, function() {
            gPane.addBreakpoint({ url: currentUrl, line: 16 }, function() {
              gPane.addBreakpoint({ url: currentUrl, line: 17 }, function() {
                gPane.addBreakpoint({ url: currentUrl, line: 18 }, function() {
                  gPane.addBreakpoint({ url: currentUrl, line: 19 }, function() {
                    gPane.addBreakpoint({ url: currentUrl, line: 20 }, function() {
                      gPane.addBreakpoint({ url: currentUrl, line: 21 }, function() {
                        gPane.addBreakpoint({ url: currentUrl, line: 22 }, function() {
                          gPane.addBreakpoint({ url: currentUrl, line: 23 }, function() {
                            gPane.addBreakpoint({ url: currentUrl, line: 24 }, function() {
                              performTest();
                            }, {
                              conditionalExpression: "b"
                            });
                          }, {
                            conditionalExpression: "a !== null"
                          });
                        }, {
                          conditionalExpression: "a !== undefined"
                        });
                      }, {
                        conditionalExpression: "a"
                      });
                    }, {
                      conditionalExpression: "(function() { return false; })()"
                    });
                  }, {
                    conditionalExpression: "(function() {})"
                  });
                }, {
                  conditionalExpression: "({})"
                });
              }, {
                conditionalExpression: "/regexp/"
              });
            }, {
              conditionalExpression: "'nasu'"
            });
          }, {
            conditionalExpression: "true"
          });
        }, {
          conditionalExpression: "42"
        });
      }, {
        conditionalExpression: "null"
      });
    }, {
      conditionalExpression: "undefined"
    });
  }

  function testReload()
  {
    info("Testing reload...");

    function _get(url, line) {
      return [
        gDebugger.DebuggerView.Sources.getBreakpoint(url, line),
        gDebugger.DebuggerController.Breakpoints.getBreakpoint(url, line),
        url,
        line,
      ];
    }

    gDebugger.addEventListener("Debugger:SourceShown", function _onSourceShown() {
      gDebugger.removeEventListener("Debugger:SourceShown", _onSourceShown);

      waitForBreakpoints(13, function() {
        testBreakpoint.apply(this, _get(gSources.selectedValue, 14));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 15));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 16));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 17));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 18));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 19));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 21));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 22));
        testBreakpoint.apply(this, _get(gSources.selectedValue, 23));

      isnot(gSources.selectedItem, null,
        "There should be a selected script in the scripts pane.")
      is(gSources.selectedBreakpointItem, null,
        "There should be no selected breakpoint in the scripts pane.")
      is(gSources.selectedBreakpointClient, null,
        "There should be no selected client in the scripts pane.");
      is(gSources._conditionalPopupVisible, false,
        "The breakpoint conditional expression popup should not be shown.");

        closeDebuggerAndFinish();
      });
    });

    finalCheck();
    gDebuggee.location.reload();
  }

  function finalCheck() {
    isnot(gEditor.getText().indexOf("ermahgerd"), -1,
      "The correct script is still loaded.");
    is(gSources.selectedValue, gSources.values[0],
      "The correct script is still selected");
  }

  function resume(expected, callback) {
    gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
      waitForBreakpoint(expected, callback);
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
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

  function waitForBreakpoints(total, callback)
  {
    // Poll every few milliseconds until the breakpoints are retrieved.
    let count = 0;
    let intervalID = window.setInterval(function() {
      info("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the breakpoints.");
        window.clearInterval(intervalID);
        return closeDebuggerAndFinish();
      }
      if (gSources.widget._list.querySelectorAll(".dbg-breakpoint").length != total) {
        return;
      }
      // We got all the breakpoints, it's safe to callback.
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
