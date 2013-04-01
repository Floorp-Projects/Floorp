/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_function-search-02.html";

/**
 * Tests if the function searching works properly.
 */

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gEditor = null;
let gSources = null;
let gSearchBox = null;
let gFilteredFunctions = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);
      Services.tm.currentThread.dispatch({ run: testFunctionsFilter }, 0);
    });
  });
}

function testFunctionsFilter()
{
  gEditor = gDebugger.DebuggerView.editor;
  gSources = gDebugger.DebuggerView.Sources;
  gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
  gFilteredFunctions = gDebugger.DebuggerView.FilteredFunctions;

  htmlSearch(function() {
    showSource("test-function-search-01.js", function() {
      firstSearch(function() {
        showSource("test-function-search-02.js", function() {
          secondSearch(function() {
            showSource("test-function-search-03.js", function() {
              thirdSearch(function() {
                saveSearch(function() {
                  filterSearch(function() {
                    bogusSearch(function() {
                      anotherSearch(function() {
                        emptySearch(function() {
                          closeDebuggerAndFinish();
                        });
                      })
                    })
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

function htmlSearch(callback) {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    ok(gFilteredFunctions.selectedValue,
      "An item should be selected in the filtered functions view");
    ok(gFilteredFunctions.selectedLabel,
      "An item should be selected in the filtered functions view");

    let url = gSources.selectedValue;
    if (url.indexOf("-02.html") != -1) {

      executeSoon(function() {
        let expectedResults = [
          ["inline", "-02.html", "", 16, 15],
          ["arrow", "-02.html", "", 17, 10],
          ["foo", "-02.html", "", 19, 10],
          ["foo2", "-02.html", "", 20, 10],
          ["bar2", "-02.html", "", 20, 17]
        ];

        for (let [label, value, description, line, col] of expectedResults) {
          is(gFilteredFunctions.selectedItem.label,
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
          ok(gFilteredFunctions.selectedItem.value.contains(value),
            "The corect value (" + value + ") is attached.");
          is(gFilteredFunctions.selectedItem.description, description,
            "The corect description (" + description + ") is currently shown.");

          info("Editor caret position: " + gEditor.getCaretPosition().toSource());
          ok(gEditor.getCaretPosition().line == line &&
             gEditor.getCaretPosition().col == col,
            "The editor didn't jump to the correct line.");

          ok(gSources.selectedLabel, label,
            "The current source isn't the correct one, according to the label.");
          ok(gSources.selectedValue, value,
            "The current source isn't the correct one, according to the value.");

          EventUtils.sendKey("DOWN", gDebugger);
        }

        ok(gEditor.getCaretPosition().line == expectedResults[0][3] &&
           gEditor.getCaretPosition().col == expectedResults[0][4],
          "The editor didn't jump to the correct line again.");

        executeSoon(callback);
      });
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  write("@");
}

function firstSearch(callback) {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    ok(gFilteredFunctions.selectedValue,
      "An item should be selected in the filtered functions view");
    ok(gFilteredFunctions.selectedLabel,
      "An item should be selected in the filtered functions view");

    let url = gSources.selectedValue;
    if (url.indexOf("-01.js") != -1) {

      executeSoon(function() {
        let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
        let expectedResults = [
          ["test", "-01.js", "", 3, 9],
          ["anonymousExpression", "-01.js", "test.prototype", 8, 2],
          ["namedExpression" + s + "NAME", "-01.js", "test.prototype", 10, 2],
          ["a_test", "-01.js", "foo", 21, 2],
          ["n_test" + s + "x", "-01.js", "foo", 23, 2],
          ["a_test", "-01.js", "foo.sub", 26, 4],
          ["n_test" + s + "y", "-01.js", "foo.sub", 28, 4],
          ["a_test", "-01.js", "foo.sub.sub", 31, 6],
          ["n_test" + s + "z", "-01.js", "foo.sub.sub", 33, 6],
          ["test_SAME_NAME", "-01.js", "foo.sub.sub.sub", 36, 8]
        ];

        for (let [label, value, description, line, col] of expectedResults) {
          is(gFilteredFunctions.selectedItem.label,
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
          ok(gFilteredFunctions.selectedItem.value.contains(value),
            "The corect value (" + value + ") is attached.");
          is(gFilteredFunctions.selectedItem.description, description,
            "The corect description (" + description + ") is currently shown.");

          info("Editor caret position: " + gEditor.getCaretPosition().toSource());
          ok(gEditor.getCaretPosition().line == line &&
             gEditor.getCaretPosition().col == col,
            "The editor didn't jump to the correct line.");

          ok(gSources.selectedLabel, label,
            "The current source isn't the correct one, according to the label.");
          ok(gSources.selectedValue, value,
            "The current source isn't the correct one, according to the value.");

          EventUtils.sendKey("DOWN", gDebugger);
        }

        ok(gEditor.getCaretPosition().line == expectedResults[0][3] &&
           gEditor.getCaretPosition().col == expectedResults[0][4],
          "The editor didn't jump to the correct line again.");

        executeSoon(callback);
      });
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  write("@");
}

function secondSearch(callback) {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    ok(gFilteredFunctions.selectedValue,
      "An item should be selected in the filtered functions view");
    ok(gFilteredFunctions.selectedLabel,
      "An item should be selected in the filtered functions view");

    let url = gSources.selectedValue;
    if (url.indexOf("-02.js") != -1) {

      executeSoon(function() {
        let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
        let expectedResults = [
          ["test2", "-02.js", "", 3, 4],
          ["test3" + s + "test3_NAME", "-02.js", "", 7, 4],
          ["test4_SAME_NAME", "-02.js", "", 10, 4],
          ["x" + s + "X", "-02.js", "test.prototype", 13, 0],
          ["y" + s + "Y", "-02.js", "test.prototype.sub", 15, 0],
          ["z" + s + "Z", "-02.js", "test.prototype.sub.sub", 17, 0],
          ["t", "-02.js", "test.prototype.sub.sub.sub", 19, 0],
          ["x", "-02.js", "", 19, 31],
          ["y", "-02.js", "", 19, 40],
          ["z", "-02.js", "", 19, 49]
        ];

        for (let [label, value, description, line, col] of expectedResults) {
          is(gFilteredFunctions.selectedItem.label,
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
          ok(gFilteredFunctions.selectedItem.value.contains(value),
            "The corect value (" + value + ") is attached.");
          is(gFilteredFunctions.selectedItem.description, description,
            "The corect description (" + description + ") is currently shown.");

          info("Editor caret position: " + gEditor.getCaretPosition().toSource());
          ok(gEditor.getCaretPosition().line == line &&
             gEditor.getCaretPosition().col == col,
            "The editor didn't jump to the correct line.");

          ok(gSources.selectedLabel, label,
            "The current source isn't the correct one, according to the label.");
          ok(gSources.selectedValue, value,
            "The current source isn't the correct one, according to the value.");

          EventUtils.sendKey("DOWN", gDebugger);
        }

        ok(gEditor.getCaretPosition().line == expectedResults[0][3] &&
           gEditor.getCaretPosition().col == expectedResults[0][4],
          "The editor didn't jump to the correct line again.");

        executeSoon(callback);
      });
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  write("@");
}

function thirdSearch(callback) {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    ok(gFilteredFunctions.selectedValue,
      "An item should be selected in the filtered functions view");
    ok(gFilteredFunctions.selectedLabel,
      "An item should be selected in the filtered functions view");

    let url = gSources.selectedValue;
    if (url.indexOf("-03.js") != -1) {

      executeSoon(function() {
        let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
        let expectedResults = [
          ["namedEventListener", "-03.js", "", 3, 42],
          ["a" + s + "A", "-03.js", "bar", 9, 4],
          ["b" + s + "B", "-03.js", "bar.alpha", 14, 4],
          ["c" + s + "C", "-03.js", "bar.alpha.beta", 19, 4],
          ["d" + s + "D", "-03.js", "theta", 24, 4],
          ["fun", "-03.js", "", 28, 6],
          ["foo", "-03.js", "", 28, 12],
          ["bar", "-03.js", "", 28, 18],
          ["t_foo", "-03.js", "", 28, 24],
          ["w_bar" + s + "baz", "-03.js", "window", 28, 37]
        ];

        for (let [label, value, description, line, col] of expectedResults) {
          is(gFilteredFunctions.selectedItem.label,
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
          ok(gFilteredFunctions.selectedItem.value.contains(value),
            "The corect value (" + value + ") is attached.");
          is(gFilteredFunctions.selectedItem.description, description,
            "The corect description (" + description + ") is currently shown.");

          info("Editor caret position: " + gEditor.getCaretPosition().toSource());
          ok(gEditor.getCaretPosition().line == line &&
             gEditor.getCaretPosition().col == col,
            "The editor didn't jump to the correct line.");

          ok(gSources.selectedLabel, label,
            "The current source isn't the correct one, according to the label.");
          ok(gSources.selectedValue, value,
            "The current source isn't the correct one, according to the value.");

          EventUtils.sendKey("DOWN", gDebugger);
        }

        ok(gEditor.getCaretPosition().line == expectedResults[0][3] &&
           gEditor.getCaretPosition().col == expectedResults[0][4],
          "The editor didn't jump to the correct line again.");

        executeSoon(callback);
      });
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  write("@");
}

function filterSearch(callback) {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    ok(gFilteredFunctions.selectedValue,
      "An item should be selected in the filtered functions view");
    ok(gFilteredFunctions.selectedLabel,
      "An item should be selected in the filtered functions view");

    let url = gSources.selectedValue;
    if (url.indexOf("-03.js") != -1) {

      executeSoon(function() {
        let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
        let expectedResults = [
          ["namedEventListener", "-03.js", "", 3, 42],
          ["a" + s + "A", "-03.js", "bar", 9, 4],
          ["bar", "-03.js", "", 28, 18],
          ["w_bar" + s + "baz", "-03.js", "window", 28, 37],
          ["test3" + s + "test3_NAME", "-02.js", "", 7, 4],
          ["test4_SAME_NAME", "-02.js", "", 10, 4],
          ["anonymousExpression", "-01.js", "test.prototype", 8, 2],
          ["namedExpression" + s + "NAME", "-01.js", "test.prototype", 10, 2],
          ["a_test", "-01.js", "foo", 21, 2],
          ["a_test", "-01.js", "foo.sub", 26, 4]
        ];

        for (let [label, value, description, line, col] of expectedResults) {
          is(gFilteredFunctions.selectedItem.label,
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
          ok(gFilteredFunctions.selectedItem.value.contains(value),
            "The corect value (" + value + ") is attached.");
          is(gFilteredFunctions.selectedItem.description, description,
            "The corect description (" + description + ") is currently shown.");

          info("Editor caret position: " + gEditor.getCaretPosition().toSource());
          ok(gEditor.getCaretPosition().line == line &&
             gEditor.getCaretPosition().col == col,
            "The editor didn't jump to the correct line.");

          ok(gSources.selectedLabel, label,
            "The current source isn't the correct one, according to the label.");
          ok(gSources.selectedValue, value,
            "The current source isn't the correct one, according to the value.");

          EventUtils.sendKey("DOWN", gDebugger);
        }

        ok(gEditor.getCaretPosition().line == expectedResults[0][3] &&
           gEditor.getCaretPosition().col == expectedResults[0][4],
          "The editor didn't jump to the correct line again.");

        executeSoon(callback);
      });
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  write("@a");
}

function bogusSearch(callback) {
  gDebugger.addEventListener("popuphidden", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);

    ok(true, "Popup was successfully hidden after no matches were found!");
    executeSoon(callback);
  });

  write("@bogus");
}

function anotherSearch(callback) {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);

    ok(true, "Popup was successfully shown after some matches were found!");
    executeSoon(callback);
  });

  write("@NAME");
}

function emptySearch(callback) {
  gDebugger.addEventListener("popuphidden", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);

    ok(true, "Popup was successfully hidden when nothing was searched!");
    executeSoon(callback);
  });

  clear();
}

function showSource(label, callback) {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    executeSoon(callback);
  });
  gSources.selectedLabel = label;
}

function saveSearch(callback) {
  gDebugger.addEventListener("popuphidden", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    executeSoon(callback);
  });
  if (Math.random() >= 0.5) {
    EventUtils.sendKey("RETURN", gDebugger);
  } else {
    EventUtils.sendMouseEvent({ type: "click" },
      gFilteredFunctions.selectedItem.target,
      gDebugger);
  }
}

function waitForCaretPos(number, callback)
{
  // Poll every few milliseconds until the source editor line is active.
  let count = 0;
  let intervalID = window.setInterval(function() {
    info("count: " + count + " ");
    info("caret: " + gEditor.getCaretPosition().line);
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

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();
  append(text);
}

function backspace(times) {
  for (let i = 0; i < times; i++) {
    EventUtils.sendKey("BACK_SPACE", gDebugger);
  }
}

function append(text) {
  gSearchBox.focus();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i], gDebugger);
  }
  info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gSearchBox = null;
  gFilteredFunctions = null;
});
