/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_update-editor-mode.html";

/**
 * Tests basic functionality of scripts filtering (file search) helper UI.
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gEditor = null;
var gSources = null;
var gSearchBox = null;
var gFilteredSources = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);
      Services.tm.currentThread.dispatch({ run: testScriptSearching }, 0);
    });
  });
}

function testScriptSearching() {
  gEditor = gDebugger.DebuggerView.editor;
  gSources = gDebugger.DebuggerView.Sources;
  gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
  gFilteredSources = gDebugger.DebuggerView.FilteredSources;

  firstSearch();
}

function firstSearch() {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    for (let i = 0; i < gFilteredSources.itemCount; i++) {
      is(gFilteredSources.labels[i],
         gDebugger.SourceUtils.trimUrlLength(gSources.labels[i]),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.values[i],
         gDebugger.SourceUtils.trimUrlLength(gSources.values[i], 0, "start"),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].label,
         gDebugger.SourceUtils.trimUrlLength(gSources.labels[i]),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].value,
         gDebugger.SourceUtils.trimUrlLength(gSources.values[i], 0, "start"),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].attachment.fullLabel, gSources.labels[i],
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].attachment.fullValue, gSources.values[i],
        "The filtered sources view should have the correct values.");
    }

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        secondSearch();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  write(".");
}

function secondSearch() {
  let sourceshown = false;
  let popupshown = false;
  let proceeded = false;

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent1(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent1);
    sourceshown = true;
    executeSoon(proceed);
  });
  gDebugger.addEventListener("popupshown", function _onEvent2(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent2);
    popupshown = true;
    executeSoon(proceed);
  });

  function proceed() {
    if (!sourceshown || !popupshown || proceeded) {
      return;
    }
    proceeded = true;
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 1,
      "The filtered sources view should have 1 items available.");
    is(gFilteredSources.visibleItems.length, 1,
      "The filtered sources view should have 1 items visible.");

    for (let i = 0; i < gFilteredSources.itemCount; i++) {
      is(gFilteredSources.labels[i],
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.values[i],
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].value, 0, "start"),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].label,
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].value,
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].value, 0, "start"),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].attachment.fullLabel, gSources.visibleItems[i].label,
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].attachment.fullValue, gSources.visibleItems[i].value,
        "The filtered sources view should have the correct values.");
    }

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 1,
          "Not all the correct scripts are shown after the search.");

        thirdSearch();
      });
    } else {
      ok(false, "How did you get here?");
    }
  }
  write(".-0");
}

function thirdSearch() {
  let sourceshown = false;
  let popupshown = false;
  let proceeded = false;

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent1(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent1);
    sourceshown = true;
    executeSoon(proceed);
  });
  gDebugger.addEventListener("popupshown", function _onEvent2(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent2);
    popupshown = true;
    executeSoon(proceed);
  });

  function proceed() {
    if (!sourceshown || !popupshown || proceeded) {
      return;
    }
    proceeded = true;
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    for (let i = 0; i < gFilteredSources.itemCount; i++) {
      is(gFilteredSources.labels[i],
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.values[i],
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].value, 0, "start"),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].label,
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].value,
         gDebugger.SourceUtils.trimUrlLength(gSources.visibleItems[i].value, 0, "start"),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].attachment.fullLabel, gSources.visibleItems[i].label,
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].attachment.fullValue, gSources.visibleItems[i].value,
        "The filtered sources view should have the correct values.");
    }

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goDown();
      });
    } else {
      ok(false, "How did you get here?");
    }
  }
  write(".-");
}

function goDown() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("test-editor-mode") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goDownAgain();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("DOWN", gDebugger);
}

function goDownAgain() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goDownAndWrap();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.synthesizeKey("g", { metaKey: true }, gDebugger);
}

function goDownAndWrap() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goUpAndWrap();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.synthesizeKey("n", { ctrlKey: true }, gDebugger);
}

function goUpAndWrap() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        clickAndSwitch();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("UP", gDebugger);
}

function clickAndSwitch() {
  let sourceshown = false;
  let popuphidden = false;
  let popupshown = false;
  let reopened = false;
  let proceeded = false;

  gDebugger.addEventListener("popuphidden", function _onEvent2(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent2);
    info("Popup was hidden...");
    popuphidden = true;

    gDebugger.addEventListener("popupshown", function _onEvent3(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent3);
      info("Popup was shown...");
      popupshown = true;

      proceed();
    });

    reopen();
  });

  function reopen() {
    if (!sourceshown || !popuphidden || reopened) {
      return;
    }
    info("Reopening popup...");
    reopened = true;
    append(".-");
  }

  function proceed() {
    if (!sourceshown || !popuphidden || !popupshown || proceeded) {
      return;
    }
    info("Proceeding with next test...");
    proceeded = true;
    executeSoon(clickAndSwitchAgain);
  }

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        info("Source was shown and verified");
        sourceshown = true;
        reopen();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });

  ok(gFilteredSources.widget._parent.querySelectorAll(".results-panel-item")[0]
     .classList.contains("results-panel-item"),
     "The first visible item target isn't the correct one.");

  EventUtils.sendMouseEvent({ type: "click" },
    gFilteredSources.widget._parent.querySelectorAll(".results-panel-item")[0],
    gDebugger);
}

function clickAndSwitchAgain() {
  let sourceshown = false;
  let popuphidden = false;
  let popupshown = false;
  let reopened = false;
  let proceeded = false;

  gDebugger.addEventListener("popuphidden", function _onEvent2(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent2);
    info("Popup was hidden...");
    popuphidden = true;

    gDebugger.addEventListener("popupshown", function _onEvent3(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent3);
      info("Popup was shown...");
      popupshown = true;

      proceed();
    });

    reopen();
  });

  function reopen() {
    if (!sourceshown || !popuphidden || reopened) {
      return;
    }
    info("Reopening popup...");
    reopened = true;
    append(".-");
  }

  function proceed() {
    if (!sourceshown || !popuphidden || !popupshown || proceeded) {
      return;
    }
    info("Proceeding with next test...");
    proceeded = true;
    executeSoon(switchFocusWithEscape);
  }

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.itemCount, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedValue, 0, "start"),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gSources.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gSources.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        info("Source was shown and verified");
        sourceshown = true;
        reopen();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });

  ok(gFilteredSources.widget._parent.querySelectorAll(".results-panel-item")[2]
     .classList.contains("results-panel-item"),
     "The first visible item target isn't the correct one.");

  EventUtils.sendMouseEvent({ type: "click" },
    gFilteredSources.widget._parent.querySelectorAll(".results-panel-item")[2],
    gDebugger);
}

function switchFocusWithEscape() {
  gDebugger.addEventListener("popuphidden", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);

    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gSources.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        focusAgainAfterEscape();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("ESCAPE", gDebugger);
}

function focusAgainAfterEscape() {
  gDebugger.addEventListener("popupshown", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);

    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gSources.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 1,
          "Not all the correct scripts are shown after the search.");

        switchFocusWithReturn();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  append("0");
}

function switchFocusWithReturn() {
  gDebugger.addEventListener("popuphidden", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);

    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gSources.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gSources.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        closeDebuggerAndFinish();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("RETURN", gDebugger);
}

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();
  append(text);
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
  gFilteredSources = null;
});
