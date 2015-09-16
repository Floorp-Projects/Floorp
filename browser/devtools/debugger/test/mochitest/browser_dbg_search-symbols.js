/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the function searching works properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gSearchBox, gFilteredFunctions;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
    gFilteredFunctions = gDebugger.DebuggerView.Filtering.FilteredFunctions;

    waitForSourceShown(gPanel, "-01.js")
      .then(() => showSource("doc_function-search.html"))
      .then(htmlSearch)
      .then(() => showSource("code_function-search-01.js"))
      .then(firstJsSearch)
      .then(() => showSource("code_function-search-02.js"))
      .then(secondJsSearch)
      .then(() => showSource("code_function-search-03.js"))
      .then(thirdJsSearch)
      .then(saveSearch)
      .then(filterSearch)
      .then(bogusSearch)
      .then(incrementalSearch)
      .then(emptySearch)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function htmlSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    writeInfo();

    is(gFilteredFunctions.selectedIndex, 0,
      "An item should be selected in the filtered functions view (1).");
    ok(gFilteredFunctions.selectedItem,
      "An item should be selected in the filtered functions view (2).");

    if (gSources.selectedItem.attachment.source.url.indexOf(".html") != -1) {
      let expectedResults = [
        ["inline", ".html", "", 19, 16],
        ["arrow", ".html", "", 20, 11],
        ["foo", ".html", "", 22, 11],
        ["foo2", ".html", "", 23, 11],
        ["bar2", ".html", "", 23, 18]
      ];

      for (let [label, value, description, line, column] of expectedResults) {
        let target = gFilteredFunctions.selectedItem.target;

        if (label) {
          is(target.querySelector(".results-panel-item-label").getAttribute("value"),
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
        } else {
          ok(!target.querySelector(".results-panel-item-label"),
            "Shouldn't create empty label nodes.");
        }
        if (value) {
          ok(target.querySelector(".results-panel-item-label-below").getAttribute("value").includes(value),
            "The corect value (" + value + ") is attached.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-below"),
            "Shouldn't create empty label nodes.");
        }
        if (description) {
          is(target.querySelector(".results-panel-item-label-before").getAttribute("value"), description,
            "The corect description (" + description + ") is currently shown.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-before"),
            "Shouldn't create empty label nodes.");
        }

        ok(isCaretPos(gPanel, line, column),
          "The editor didn't jump to the correct line.");

        EventUtils.sendKey("DOWN", gDebugger);
      }

      ok(isCaretPos(gPanel, expectedResults[0][3], expectedResults[0][4]),
        "The editor didn't jump to the correct line again.");

      deferred.resolve();
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  setText(gSearchBox, "@");
  return deferred.promise;
}

function firstJsSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    writeInfo();

    is(gFilteredFunctions.selectedIndex, 0,
      "An item should be selected in the filtered functions view (1).");
    ok(gFilteredFunctions.selectedItem,
      "An item should be selected in the filtered functions view (2).");

    if (gSources.selectedItem.attachment.source.url.indexOf("-01.js") != -1) {
      let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
      let expectedResults = [
        ["test", "-01.js", "", 4, 10],
        ["anonymousExpression", "-01.js", "test.prototype", 9, 3],
        ["namedExpression" + s + "NAME", "-01.js", "test.prototype", 11, 3],
        ["a_test", "-01.js", "foo", 22, 3],
        ["n_test" + s + "x", "-01.js", "foo", 24, 3],
        ["a_test", "-01.js", "foo.sub", 27, 5],
        ["n_test" + s + "y", "-01.js", "foo.sub", 29, 5],
        ["a_test", "-01.js", "foo.sub.sub", 32, 7],
        ["n_test" + s + "z", "-01.js", "foo.sub.sub", 34, 7],
        ["test_SAME_NAME", "-01.js", "foo.sub.sub.sub", 37, 9]
      ];

      for (let [label, value, description, line, column] of expectedResults) {
        let target = gFilteredFunctions.selectedItem.target;

        if (label) {
          is(target.querySelector(".results-panel-item-label").getAttribute("value"),
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
        } else {
          ok(!target.querySelector(".results-panel-item-label"),
            "Shouldn't create empty label nodes.");
        }
        if (value) {
          ok(target.querySelector(".results-panel-item-label-below").getAttribute("value").includes(value),
            "The corect value (" + value + ") is attached.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-below"),
            "Shouldn't create empty label nodes.");
        }
        if (description) {
          is(target.querySelector(".results-panel-item-label-before").getAttribute("value"), description,
            "The corect description (" + description + ") is currently shown.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-before"),
            "Shouldn't create empty label nodes.");
        }

        ok(isCaretPos(gPanel, line, column),
          "The editor didn't jump to the correct line.");

        EventUtils.sendKey("DOWN", gDebugger);
      }

      ok(isCaretPos(gPanel, expectedResults[0][3], expectedResults[0][4]),
        "The editor didn't jump to the correct line again.");

      deferred.resolve()
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  setText(gSearchBox, "@");
  return deferred.promise;
}

function secondJsSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    writeInfo();

    is(gFilteredFunctions.selectedIndex, 0,
      "An item should be selected in the filtered functions view (1).");
    ok(gFilteredFunctions.selectedItem,
      "An item should be selected in the filtered functions view (2).");

    if (gSources.selectedItem.attachment.source.url.indexOf("-02.js") != -1) {
      let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
      let expectedResults = [
        ["test2", "-02.js", "", 4, 5],
        ["test3" + s + "test3_NAME", "-02.js", "", 8, 5],
        ["test4_SAME_NAME", "-02.js", "", 11, 5],
        ["x" + s + "X", "-02.js", "test.prototype", 14, 1],
        ["y" + s + "Y", "-02.js", "test.prototype.sub", 16, 1],
        ["z" + s + "Z", "-02.js", "test.prototype.sub.sub", 18, 1],
        ["t", "-02.js", "test.prototype.sub.sub.sub", 20, 1],
        ["x", "-02.js", "this", 20, 32],
        ["y", "-02.js", "this", 20, 41],
        ["z", "-02.js", "this", 20, 50]
      ];

      for (let [label, value, description, line, column] of expectedResults) {
        let target = gFilteredFunctions.selectedItem.target;

        if (label) {
          is(target.querySelector(".results-panel-item-label").getAttribute("value"),
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
        } else {
          ok(!target.querySelector(".results-panel-item-label"),
            "Shouldn't create empty label nodes.");
        }
        if (value) {
          ok(target.querySelector(".results-panel-item-label-below").getAttribute("value").includes(value),
            "The corect value (" + value + ") is attached.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-below"),
            "Shouldn't create empty label nodes.");
        }
        if (description) {
          is(target.querySelector(".results-panel-item-label-before").getAttribute("value"), description,
            "The corect description (" + description + ") is currently shown.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-before"),
            "Shouldn't create empty label nodes.");
        }

        ok(isCaretPos(gPanel, line, column),
          "The editor didn't jump to the correct line.");

        EventUtils.sendKey("DOWN", gDebugger);
      }

      ok(isCaretPos(gPanel, expectedResults[0][3], expectedResults[0][4]),
        "The editor didn't jump to the correct line again.");

      deferred.resolve();
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  setText(gSearchBox, "@");
  return deferred.promise;
}

function thirdJsSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    writeInfo();

    is(gFilteredFunctions.selectedIndex, 0,
      "An item should be selected in the filtered functions view (1).");
    ok(gFilteredFunctions.selectedItem,
      "An item should be selected in the filtered functions view (2).");

    if (gSources.selectedItem.attachment.source.url.indexOf("-03.js") != -1) {
      let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
      let expectedResults = [
        ["namedEventListener", "-03.js", "", 4, 43],
        ["a" + s + "A", "-03.js", "bar", 10, 5],
        ["b" + s + "B", "-03.js", "bar.alpha", 15, 5],
        ["c" + s + "C", "-03.js", "bar.alpha.beta", 20, 5],
        ["d" + s + "D", "-03.js", "this.theta", 25, 5],
        ["fun", "-03.js", "", 29, 7],
        ["foo", "-03.js", "", 29, 13],
        ["bar", "-03.js", "", 29, 19],
        ["t_foo", "-03.js", "this", 29, 25],
        ["w_bar" + s + "baz", "-03.js", "window", 29, 38]
      ];

      for (let [label, value, description, line, column] of expectedResults) {
        let target = gFilteredFunctions.selectedItem.target;

        if (label) {
          is(target.querySelector(".results-panel-item-label").getAttribute("value"),
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
        } else {
          ok(!target.querySelector(".results-panel-item-label"),
            "Shouldn't create empty label nodes.");
        }
        if (value) {
          ok(target.querySelector(".results-panel-item-label-below").getAttribute("value").includes(value),
            "The corect value (" + value + ") is attached.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-below"),
            "Shouldn't create empty label nodes.");
        }
        if (description) {
          is(target.querySelector(".results-panel-item-label-before").getAttribute("value"), description,
            "The corect description (" + description + ") is currently shown.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-before"),
            "Shouldn't create empty label nodes.");
        }

        ok(isCaretPos(gPanel, line, column),
          "The editor didn't jump to the correct line.");

        EventUtils.sendKey("DOWN", gDebugger);
      }

      ok(isCaretPos(gPanel, expectedResults[0][3], expectedResults[0][4]),
        "The editor didn't jump to the correct line again.");

      deferred.resolve();
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  setText(gSearchBox, "@");
  return deferred.promise;
}

function filterSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    writeInfo();

    is(gFilteredFunctions.selectedIndex, 0,
      "An item should be selected in the filtered functions view (1).");
    ok(gFilteredFunctions.selectedItem,
      "An item should be selected in the filtered functions view (2).");

    if (gSources.selectedItem.attachment.source.url.indexOf("-03.js") != -1) {
      let s = " " + gDebugger.L10N.getStr("functionSearchSeparatorLabel") + " ";
      let expectedResults = [
        ["namedEventListener", "-03.js", "", 4, 43],
        ["bar", "-03.js", "", 29, 19],
        ["w_bar" + s + "baz", "-03.js", "window", 29, 38],
        ["anonymousExpression", "-01.js", "test.prototype", 9, 3],
        ["namedExpression" + s + "NAME", "-01.js", "test.prototype", 11, 3],
        ["arrow", ".html", "", 20, 11],
        ["bar2", ".html", "", 23, 18]
      ];

      for (let [label, value, description, line, column] of expectedResults) {
        let target = gFilteredFunctions.selectedItem.target;

        if (label) {
          is(target.querySelector(".results-panel-item-label").getAttribute("value"),
            gDebugger.SourceUtils.trimUrlLength(label + "()"),
            "The corect label (" + label + ") is currently selected.");
        } else {
          ok(!target.querySelector(".results-panel-item-label"),
            "Shouldn't create empty label nodes.");
        }
        if (value) {
          ok(target.querySelector(".results-panel-item-label-below").getAttribute("value").includes(value),
            "The corect value (" + value + ") is attached.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-below"),
            "Shouldn't create empty label nodes.");
        }
        if (description) {
          is(target.querySelector(".results-panel-item-label-before").getAttribute("value"), description,
            "The corect description (" + description + ") is currently shown.");
        } else {
          ok(!target.querySelector(".results-panel-item-label-before"),
            "Shouldn't create empty label nodes.");
        }

        ok(isCaretPos(gPanel, line, column),
          "The editor didn't jump to the correct line.");

        EventUtils.sendKey("DOWN", gDebugger);
      }

      ok(isCaretPos(gPanel, expectedResults[0][3], expectedResults[0][4]),
        "The editor didn't jump to the correct line again.");

      deferred.resolve();
    } else {
      ok(false, "How did you get here? Go away, you.");
    }
  });

  setText(gSearchBox, "@r");
  return deferred.promise;
}

function bogusSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popuphidden").then(() => {
    ok(true, "Popup was successfully hidden after no matches were found.");
    deferred.resolve();
  });

  setText(gSearchBox, "@bogus");
  return deferred.promise;
}

function incrementalSearch() {
  let deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    ok(true, "Popup was successfully shown after some matches were found.");
    deferred.resolve();
  });

  setText(gSearchBox, "@NAME");
  return deferred.promise;
}

function emptySearch() {
  let deferred = promise.defer();

  once(gDebugger, "popuphidden").then(() => {
    ok(true, "Popup was successfully hidden when nothing was searched.");
    deferred.resolve();
  });

  clearText(gSearchBox);
  return deferred.promise;
}

function showSource(aLabel) {
  let deferred = promise.defer();

  waitForSourceShown(gPanel, aLabel).then(deferred.resolve);
  gSources.selectedItem = e => e.attachment.label == aLabel;

  return deferred.promise;
}

function saveSearch() {
  let finished = once(gDebugger, "popuphidden");

  // Either by pressing RETURN or clicking on an item in the popup,
  // the popup should hide and the item should become selected.
  let random = Math.random();
  if (random >= 0.33) {
    EventUtils.sendKey("RETURN", gDebugger);
  } else if (random >= 0.66) {
    EventUtils.sendKey("RETURN", gDebugger);
  } else {
    EventUtils.sendMouseEvent({ type: "click" },
      gFilteredFunctions.selectedItem.target,
      gDebugger);
  }

  return finished;
}

function writeInfo() {
  info("Current source url:\n" + getSelectedSourceURL(gSources));
  info("Debugger editor text:\n" + gEditor.getText());
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gSearchBox = null;
  gFilteredFunctions = null;
});
