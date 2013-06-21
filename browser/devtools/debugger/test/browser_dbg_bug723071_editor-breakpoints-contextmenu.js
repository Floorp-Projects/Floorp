/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the context menu associated with each breakpoint does what it should.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gEditor = null;
let gSources = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSources.preferredSource = EXAMPLE_URL + "test-script-switching-02.js";

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      let url = aEvent.detail.url;
      if (url.indexOf("-02") != -1) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        performTestWhileNotPaused();
      }
    });
  });

  function addBreakpoints(callback) {
    gPane.addBreakpoint({url: gSources.values[0], line: 5}, function(cl, err) {
      gPane.addBreakpoint({url: gSources.values[1], line: 6}, function(cl, err) {
        gPane.addBreakpoint({url: gSources.values[1], line: 7}, function(cl, err) {
          gPane.addBreakpoint({url: gSources.values[1], line: 8}, function(cl, err) {
            gPane.addBreakpoint({url: gSources.values[1], line: 9}, function(cl, err) {
              callback();
            });
          });
        });
      });
    });
  }

  function performTestWhileNotPaused()
  {
    info("Performing test while not paused...");

    addBreakpoints(function() {
      initialChecks();

      checkBreakpointToggleSelf(0, function() {
        checkBreakpointToggleOthers(0, function() {
          checkBreakpointToggleSelf(1, function() {
            checkBreakpointToggleOthers(1, function() {
              checkBreakpointToggleSelf(2, function() {
                checkBreakpointToggleOthers(2, function() {
                  checkBreakpointToggleSelf(3, function() {
                    checkBreakpointToggleOthers(3, function() {
                      checkBreakpointToggleSelf(4, function() {
                        checkBreakpointToggleOthers(4, function() {
                          testDeleteAll(function() {
                            performTestWhilePaused();
                          });
                        });
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  }

  function performTestWhilePaused()
  {
    info("Performing test while paused...");

    addBreakpoints(function() {
      initialChecks();

      pauseAndCheck(function() {
        checkBreakpointToggleSelf(0, function() {
          checkBreakpointToggleOthers(0, function() {
            checkBreakpointToggleSelf(1, function() {
              checkBreakpointToggleOthers(1, function() {
                checkBreakpointToggleSelf(2, function() {
                  checkBreakpointToggleOthers(2, function() {
                    checkBreakpointToggleSelf(3, function() {
                      checkBreakpointToggleOthers(3, function() {
                        checkBreakpointToggleSelf(4, function() {
                          checkBreakpointToggleOthers(4, function() {
                            testDeleteAll(function() {
                              closeDebuggerAndFinish();
                            });
                          }, true);
                        });
                      }, true);
                    });
                  }, true);
                });
              }, true);
            });
          }, true);
        });
      });
    });
  }

  function pauseAndCheck(callback) {
    gDebugger.gThreadClient.addOneTimeListener("resumed", function() {
      pauseAndCallback(function() {
        is(gSources.selectedLabel, "test-script-switching-01.js",
          "The currently selected source is incorrect (1).");
        is(gSources.selectedIndex, 1,
          "The currently selected source is incorrect (2).");

        waitForCaretPos(4, function() {
          ok(true, "The editor location is correct after pausing.");
          callback();
        });
      });
    });
  }

  function pauseAndCallback(callback) {
    let scriptShown = false;
    let framesadded = false;
    let testContinued = false;

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      let url = aEvent.detail.url;
      if (url.indexOf("-01") != -1) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        scriptShown = true;
        executeSoon(continueTest);
      }
    });

    gDebugger.gThreadClient.addOneTimeListener("framesadded", function() {
      framesadded = true;
      executeSoon(continueTest);
    });

    function continueTest() {
      if (scriptShown && framesadded && !testContinued) {
        testContinued = true;
        callback();
      }
    }

    EventUtils.sendMouseEvent({ type: "click" },
      gDebuggee.document.querySelector("button"),
      gDebuggee.window);
  }

  function initialChecks() {
    for (let source in gSources) {
      for (let breakpoint in source) {
        let { sourceLocation: url, lineNumber: line, actor } = breakpoint.attachment;

        ok(gPane.getBreakpoint(url, line),
          "All breakpoint items should have corresponding clients (1).");
        ok(breakpoint.attachment.actor,
          "All breakpoint items should have corresponding clients (2).");
        is(!!breakpoint.attachment.disabled, false,
          "All breakpoints should initially be enabled.");

        let prefix = "bp-cMenu-"; // "breakpoints context menu"
        let enableSelfId = prefix + "enableSelf-" + actor + "-menuitem";
        let disableSelfId = prefix + "disableSelf-" + actor + "-menuitem";

        is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
          "The 'Enable breakpoint' context menu item should initially be hidden'.");
        ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
          "The 'Disable breakpoint' context menu item should initially not be hidden'.");
        is(breakpoint.attachment.view.checkbox.getAttribute("checked"), "true",
          "All breakpoints should initially have a checked checkbox.");
      }
    }
  }

  function checkBreakpointToggleSelf(index, callback) {
    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[index],
      gDebugger);

    let selectedBreakpoint = gSources.selectedBreakpointItem;
    let { sourceLocation: url, lineNumber: line, actor } = selectedBreakpoint.attachment;

    ok(gPane.getBreakpoint(url, line),
      "There should be a breakpoint client available (1).");
    ok(gSources.selectedBreakpointClient,
      "There should be a breakpoint client available (2).");
    is(!!selectedBreakpoint.attachment.disabled, false,
      "The breakpoint should not be disabled yet.");

    let prefix = "bp-cMenu-"; // "breakpoints context menu"
    let enableSelfId = prefix + "enableSelf-" + actor + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + actor + "-menuitem";

    is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
      "The 'Enable breakpoint' context menu item should be hidden'.");
    ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
      "The 'Disable breakpoint' context menu item should not be hidden'.");

    waitForCaretPos(selectedBreakpoint.attachment.lineNumber - 1, function() {
      ok(true, "The editor location is correct (" + index + ").");

      gDebugger.addEventListener("Debugger:BreakpointHidden", function _onEvent(aEvent) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);

        ok(!gPane.getBreakpoint(url, line),
          "There should be no breakpoint client available (2).");
        ok(!gSources.selectedBreakpointClient,
          "There should be no breakpoint client available (3).");

        gDebugger.addEventListener("Debugger:BreakpointShown", function _onEvent(aEvent) {
          gDebugger.removeEventListener(aEvent.type, _onEvent);

          ok(gPane.getBreakpoint(url, line),
            "There should be a breakpoint client available (4).");
          ok(gSources.selectedBreakpointClient,
            "There should be a breakpoint client available (5).");

          callback();
        });

        // Test re-disabling this breakpoint.
        executeSoon(function() {
          gSources._onEnableSelf(selectedBreakpoint.attachment.actor);
          is(selectedBreakpoint.attachment.disabled, false,
            "The current breakpoint should now be enabled.")

          is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
            "The 'Enable breakpoint' context menu item should be hidden'.");
          ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
            "The 'Disable breakpoint' context menu item should not be hidden'.");
          ok(selectedBreakpoint.attachment.view.checkbox.hasAttribute("checked"),
            "The breakpoint should now be checked.");
        });
      });

      // Test disabling this breakpoint.
      executeSoon(function() {
        gSources._onDisableSelf(selectedBreakpoint.attachment.actor);
        is(selectedBreakpoint.attachment.disabled, true,
          "The current breakpoint should now be disabled.")

        ok(!gDebugger.document.getElementById(enableSelfId).hasAttribute("hidden"),
          "The 'Enable breakpoint' context menu item should not be hidden'.");
        is(gDebugger.document.getElementById(disableSelfId).getAttribute("hidden"), "true",
          "The 'Disable breakpoint' context menu item should be hidden'.");
        ok(!selectedBreakpoint.attachment.view.checkbox.hasAttribute("checked"),
          "The breakpoint should now be unchecked.");
      });
    });
  }

  function checkBreakpointToggleOthers(index, callback, whilePaused) {
    let count = 4
    gDebugger.addEventListener("Debugger:BreakpointHidden", function _onEvent(aEvent) {
      info(count + " breakpoints remain to be hidden...");
      if (!(--count)) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        ok(true, "All breakpoints except one were hidden.");

        let selectedBreakpoint = gSources.selectedBreakpointItem;
        let { sourceLocation: url, lineNumber: line, actor } = selectedBreakpoint.attachment;

        ok(gPane.getBreakpoint(url, line),
          "There should be a breakpoint client available (6).");
        ok(gSources.selectedBreakpointClient,
          "There should be a breakpoint client available (7).");
        is(!!selectedBreakpoint.attachment.disabled, false,
          "The targetted breakpoint should not have been disabled.");

        for (let source in gSources) {
          for (let otherBreakpoint in source) {
            if (otherBreakpoint != selectedBreakpoint) {
              ok(!gPane.getBreakpoint(
                otherBreakpoint.attachment.sourceLocation,
                otherBreakpoint.attachment.lineNumber),
                "There should be no breakpoint client for a disabled breakpoint (8).");
              is(otherBreakpoint.attachment.disabled, true,
                "Non-targetted breakpoints should have been disabled (9).");
            }
          }
        }

        count = 4;
        gDebugger.addEventListener("Debugger:BreakpointShown", function _onEvent(aEvent) {
          info(count + " breakpoints remain to be reshown...");
          if (!(--count)) {
            gDebugger.removeEventListener(aEvent.type, _onEvent);
            ok(true, "All breakpoints are now reshown.");

            for (let source in gSources) {
              for (let someBreakpoint in source) {
                ok(gPane.getBreakpoint(
                  someBreakpoint.attachment.sourceLocation,
                  someBreakpoint.attachment.lineNumber),
                  "There should be a breakpoint client for all enabled breakpoints (10).");
                is(someBreakpoint.attachment.disabled, false,
                  "All breakpoints should now have been enabled (11).");
              }
            }

            count = 5;
            gDebugger.addEventListener("Debugger:BreakpointHidden", function _onEvent(aEvent) {
              info(count + " breakpoints remain to be rehidden...");
              if (!(--count)) {
                gDebugger.removeEventListener(aEvent.type, _onEvent);
                ok(true, "All breakpoints are now rehidden.");

                for (let source in gSources) {
                  for (let someBreakpoint in source) {
                    ok(!gPane.getBreakpoint(
                      someBreakpoint.attachment.sourceLocation,
                      someBreakpoint.attachment.lineNumber),
                      "There should be no breakpoint client for a disabled breakpoint (12).");
                    is(someBreakpoint.attachment.disabled, true,
                      "All breakpoints should now have been disabled (13).");
                  }
                }

                count = 5;
                gDebugger.addEventListener("Debugger:BreakpointShown", function _onEvent(aEvent) {
                  info(count + " breakpoints remain to be reshown...");
                  if (!(--count)) {
                    gDebugger.removeEventListener(aEvent.type, _onEvent);
                    ok(true, "All breakpoints are now rehidden.");

                    for (let source in gSources) {
                      for (let someBreakpoint in source) {
                        ok(gPane.getBreakpoint(
                          someBreakpoint.attachment.sourceLocation,
                          someBreakpoint.attachment.lineNumber),
                          "There should be a breakpoint client for all enabled breakpoints (14).");
                        is(someBreakpoint.attachment.disabled, false,
                          "All breakpoints should now have been enabled (15).");
                      }
                    }

                    // Done.
                    if (!whilePaused) {
                      gDebugger.gThreadClient.addOneTimeListener("resumed", callback);
                    } else {
                      callback();
                    }
                  }
                });

                // Test re-enabling all breakpoints.
                enableAll();
              }
            });

            // Test disabling all breakpoints.
            if (!whilePaused) {
              gDebugger.gThreadClient.addOneTimeListener("resumed", disableAll);
            } else {
              disableAll();
            }
          }
        });

        // Test re-enabling other breakpoints.
        enableOthers();
      }
    });

    // Test disabling other breakpoints.
    if (!whilePaused) {
      gDebugger.gThreadClient.addOneTimeListener("resumed", disableOthers);
    } else {
      disableOthers();
    }
  }

  function testDeleteAll(callback) {
    let count = 5
    gDebugger.addEventListener("Debugger:BreakpointHidden", function _onEvent(aEvent) {
      info(count + " breakpoints remain to be hidden...");
      if (!(--count)) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        ok(true, "All breakpoints were hidden.");

        ok(!gSources.selectedBreakpointItem,
          "There should be no breakpoint item available (16).");
        ok(!gSources.selectedBreakpointClient,
          "There should be no breakpoint client available (17).");

        for (let source in gSources) {
          for (let otherBreakpoint in source) {
            ok(false, "It's a trap!");
          }
        }

        // Done.
        callback();
      }
    });

    // Test deleting all breakpoints.
    deleteAll();
  }

  function disableOthers() {
    gSources._onDisableOthers(gSources.selectedBreakpointItem.attachment.actor);
  }
  function enableOthers() {
    gSources._onEnableOthers(gSources.selectedBreakpointItem.attachment.actor);
  }
  function disableAll() {
    gSources._onDisableAll(gSources.selectedBreakpointItem.attachment.actor);
  }
  function enableAll() {
    gSources._onEnableAll(gSources.selectedBreakpointItem.attachment.actor);
  }
  function deleteAll() {
    gSources._onDeleteAll(gSources.selectedBreakpointItem.attachment.actor);
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
  });
}
