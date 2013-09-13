/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 727429: Test the debugger watch expressions.
 */

const TAB_URL = EXAMPLE_URL + "doc_watch-expressions.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gWatch, gVariables;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gWatch = gDebugger.DebuggerView.WatchExpressions;
    gVariables = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });

    waitForSourceShown(gPanel, ".html")
      .then(() => performTest())
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function performTest() {
    is(gWatch.getAllStrings().length, 0,
      "There should initially be no watch expressions.");

    addAndCheckExpressions(1, 0, "a");
    addAndCheckExpressions(2, 0, "b");
    addAndCheckExpressions(3, 0, "c");

    removeAndCheckExpression(2, 1, "a");
    removeAndCheckExpression(1, 0, "a");

    addAndCheckExpressions(2, 0, "", true);
    gEditor.focus();
    is(gWatch.getAllStrings().length, 1,
      "Empty watch expressions are automatically removed.");

    addAndCheckExpressions(2, 0, "a", true);
    gEditor.focus();
    is(gWatch.getAllStrings().length, 1,
      "Duplicate watch expressions are automatically removed.");

    addAndCheckExpressions(2, 0, "a\t", true);
    addAndCheckExpressions(2, 0, "a\r", true);
    addAndCheckExpressions(2, 0, "a\n", true);
    gEditor.focus();
    is(gWatch.getAllStrings().length, 1,
      "Duplicate watch expressions are automatically removed.");

    addAndCheckExpressions(2, 0, "\ta", true);
    addAndCheckExpressions(2, 0, "\ra", true);
    addAndCheckExpressions(2, 0, "\na", true);
    gEditor.focus();
    is(gWatch.getAllStrings().length, 1,
      "Duplicate watch expressions are automatically removed.");

    addAndCheckCustomExpression(2, 0, "bazΩΩka");
    addAndCheckCustomExpression(3, 0, "bambøøcha");

    EventUtils.sendMouseEvent({ type: "click" },
      gWatch.getItemAtIndex(0).attachment.closeNode,
      gDebugger);

    is(gWatch.getAllStrings().length, 2,
      "Watch expressions are removed when the close button is pressed.");
    is(gWatch.getAllStrings()[0], "bazΩΩka",
      "The expression at index " + 0 + " should be correct (1).");
    is(gWatch.getAllStrings()[1], "a",
      "The expression at index " + 1 + " should be correct (2).");

    EventUtils.sendMouseEvent({ type: "click" },
      gWatch.getItemAtIndex(0).attachment.closeNode,
      gDebugger);

    is(gWatch.getAllStrings().length, 1,
      "Watch expressions are removed when the close button is pressed.");
    is(gWatch.getAllStrings()[0], "a",
      "The expression at index " + 0 + " should be correct (3).");

    EventUtils.sendMouseEvent({ type: "click" },
      gWatch.getItemAtIndex(0).attachment.closeNode,
      gDebugger);

    is(gWatch.getAllStrings().length, 0,
      "Watch expressions are removed when the close button is pressed.");

    EventUtils.sendMouseEvent({ type: "click" },
      gWatch.widget._parent,
      gDebugger);

    is(gWatch.getAllStrings().length, 1,
      "Watch expressions are added when the view container is pressed.");
  }

  function addAndCheckCustomExpression(aTotal, aIndex, aString, noBlur) {
    addAndCheckExpressions(aTotal, aIndex, "", true);

    for (let i = 0; i < aString.length; i++) {
      EventUtils.sendChar(aString[i], gDebugger);
    }

    gEditor.focus();

    let element = gWatch.getItemAtIndex(aIndex).target;

    is(gWatch.getItemAtIndex(aIndex).attachment.initialExpression, "",
      "The initial expression at index " + aIndex + " should be correct (1).");
    is(gWatch.getItemForElement(element).attachment.initialExpression, "",
      "The initial expression at index " + aIndex + " should be correct (2).");

    is(gWatch.getItemAtIndex(aIndex).attachment.currentExpression, aString,
      "The expression at index " + aIndex + " should be correct (1).");
    is(gWatch.getItemForElement(element).attachment.currentExpression, aString,
      "The expression at index " + aIndex + " should be correct (2).");

    is(gWatch.getString(aIndex), aString,
      "The expression at index " + aIndex + " should be correct (3).");
    is(gWatch.getAllStrings()[aIndex], aString,
      "The expression at index " + aIndex + " should be correct (4).");
  }

  function addAndCheckExpressions(aTotal, aIndex, aString, noBlur) {
    gWatch.addExpression(aString);

    is(gWatch.getAllStrings().length, aTotal,
      "There should be " + aTotal + " watch expressions available (1).");
    is(gWatch.itemCount, aTotal,
      "There should be " + aTotal + " watch expressions available (2).");

    ok(gWatch.getItemAtIndex(aIndex),
      "The expression at index " + aIndex + " should be available.");
    is(gWatch.getItemAtIndex(aIndex).attachment.initialExpression, aString,
      "The expression at index " + aIndex + " should have an initial expression.");

    let element = gWatch.getItemAtIndex(aIndex).target;

    ok(element,
      "There should be a new expression item in the view.");
    ok(gWatch.getItemForElement(element),
      "The watch expression item should be accessible.");
    is(gWatch.getItemForElement(element), gWatch.getItemAtIndex(aIndex),
      "The correct watch expression item was accessed.");

    ok(gWatch.widget.getItemAtIndex(aIndex) instanceof XULElement,
      "The correct watch expression element was accessed (1).");
    is(element, gWatch.widget.getItemAtIndex(aIndex),
      "The correct watch expression element was accessed (2).");

    is(gWatch.getItemForElement(element).attachment.arrowNode.hidden, false,
      "The arrow node should be visible.");
    is(gWatch.getItemForElement(element).attachment.closeNode.hidden, false,
      "The close button should be visible.");
    is(gWatch.getItemForElement(element).attachment.inputNode.getAttribute("focused"), "true",
      "The textbox input should be focused.");

    is(gVariables.parentNode.scrollTop, 0,
      "The variables view should be scrolled to top");

    is(gWatch.items[0], gWatch.getItemAtIndex(aIndex),
      "The correct watch expression was added to the cache (1).");
    is(gWatch.items[0], gWatch.getItemForElement(element),
      "The correct watch expression was added to the cache (2).");

    if (!noBlur) {
      gEditor.focus();

      is(gWatch.getItemAtIndex(aIndex).attachment.initialExpression, aString,
        "The initial expression at index " + aIndex + " should be correct (1).");
      is(gWatch.getItemForElement(element).attachment.initialExpression, aString,
        "The initial expression at index " + aIndex + " should be correct (2).");

      is(gWatch.getItemAtIndex(aIndex).attachment.currentExpression, aString,
        "The expression at index " + aIndex + " should be correct (1).");
      is(gWatch.getItemForElement(element).attachment.currentExpression, aString,
        "The expression at index " + aIndex + " should be correct (2).");

      is(gWatch.getString(aIndex), aString,
        "The expression at index " + aIndex + " should be correct (3).");
      is(gWatch.getAllStrings()[aIndex], aString,
        "The expression at index " + aIndex + " should be correct (4).");
    }
  }

  function removeAndCheckExpression(aTotal, aIndex, aString) {
    gWatch.removeAt(aIndex);

    is(gWatch.getAllStrings().length, aTotal,
      "There should be " + aTotal + " watch expressions available (1).");
    is(gWatch.itemCount, aTotal,
      "There should be " + aTotal + " watch expressions available (2).");

    ok(gWatch.getItemAtIndex(aIndex),
      "The expression at index " + aIndex + " should still be available.");
    is(gWatch.getItemAtIndex(aIndex).attachment.initialExpression, aString,
      "The expression at index " + aIndex + " should still have an initial expression.");

    let element = gWatch.getItemAtIndex(aIndex).target;

    is(gWatch.getItemAtIndex(aIndex).attachment.initialExpression, aString,
      "The initial expression at index " + aIndex + " should be correct (1).");
    is(gWatch.getItemForElement(element).attachment.initialExpression, aString,
      "The initial expression at index " + aIndex + " should be correct (2).");

    is(gWatch.getItemAtIndex(aIndex).attachment.currentExpression, aString,
      "The expression at index " + aIndex + " should be correct (1).");
    is(gWatch.getItemForElement(element).attachment.currentExpression, aString,
      "The expression at index " + aIndex + " should be correct (2).");

    is(gWatch.getString(aIndex), aString,
      "The expression at index " + aIndex + " should be correct (3).");
    is(gWatch.getAllStrings()[aIndex], aString,
      "The expression at index " + aIndex + " should be correct (4).");
  }
}
