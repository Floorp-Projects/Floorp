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
var gScripts = null;
var gSearchBox = null;
var gFilteredSources = null;
var gMenulist = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
  });

  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    Services.tm.currentThread.dispatch({ run: testScriptSearching }, 0);
  });
}

function testScriptSearching() {
  gEditor = gDebugger.DebuggerView.editor;
  gScripts = gDebugger.DebuggerView.Sources;
  gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
  gFilteredSources = gDebugger.DebuggerView.FilteredSources;
  gMenulist = gScripts._container;

  firstSearch();
}

function firstSearch() {
  window.addEventListener("popupshown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    for (let i = 0; i < gFilteredSources.totalItems; i++) {
      is(gFilteredSources.labels[i],
         gDebugger.SourceUtils.trimUrlLength(gScripts.labels[i]),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.values[i],
         gDebugger.SourceUtils.trimUrlLength(gScripts.values[i]),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].label,
         gDebugger.SourceUtils.trimUrlLength(gScripts.labels[i]),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].value,
         gDebugger.SourceUtils.trimUrlLength(gScripts.values[i]),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].attachment.fullLabel, gScripts.labels[i],
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].attachment.fullValue, gScripts.values[i],
        "The filtered sources view should have the correct values.");
    }

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
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

  window.addEventListener("Debugger:SourceShown", function _onEvent1(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent1);
    sourceshown = true;
    executeSoon(proceed);
  });
  window.addEventListener("popupshown", function _onEvent2(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent2);
    popupshown = true;
    executeSoon(proceed);
  });

  function proceed() {
    if (!sourceshown || !popupshown || proceeded) {
      return;
    }
    proceeded = true;
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 1,
      "The filtered sources view should have 1 items available.");
    is(gFilteredSources.visibleItems.length, 1,
      "The filtered sources view should have 1 items visible.");

    for (let i = 0; i < gFilteredSources.totalItems; i++) {
      is(gFilteredSources.labels[i],
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.values[i],
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].value),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].label,
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].value,
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].value),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].attachment.fullLabel, gScripts.visibleItems[i].label,
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].attachment.fullValue, gScripts.visibleItems[i].value,
        "The filtered sources view should have the correct values.");
    }

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 1,
          "Not all the correct scripts are shown after the search.");

        thirdSearch();
      });
    } else {
      ok(false, "How did you get here?");
    }
  }
  append("-0")
}

function thirdSearch() {
  let sourceshown = false;
  let popupshown = false;
  let proceeded = false;

  window.addEventListener("Debugger:SourceShown", function _onEvent1(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent1);
    sourceshown = true;
    executeSoon(proceed);
  });
  window.addEventListener("popupshown", function _onEvent2(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent2);
    popupshown = true;
    executeSoon(proceed);
  });

  function proceed() {
    if (!sourceshown || !popupshown || proceeded) {
      return;
    }
    proceeded = true;
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    for (let i = 0; i < gFilteredSources.totalItems; i++) {
      is(gFilteredSources.labels[i],
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.values[i],
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].value),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].label,
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].label),
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].value,
         gDebugger.SourceUtils.trimUrlLength(gScripts.visibleItems[i].value),
        "The filtered sources view should have the correct values.");

      is(gFilteredSources.visibleItems[i].attachment.fullLabel, gScripts.visibleItems[i].label,
        "The filtered sources view should have the correct labels.");
      is(gFilteredSources.visibleItems[i].attachment.fullValue, gScripts.visibleItems[i].value,
        "The filtered sources view should have the correct values.");
    }

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goDown();
      });
    } else {
      ok(false, "How did you get here?");
    }
  }
  backspace(1)
}

function goDown() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-editor-mode") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goDownAgain();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("DOWN");
}

function goDownAgain() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goDownAndWrap();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.synthesizeKey("g", { metaKey: true });
}

function goDownAndWrap() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        goUpAndWrap();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.synthesizeKey("n", { ctrlKey: true });
}

function goUpAndWrap() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        clickAndSwitch();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("UP");
}

function clickAndSwitch() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("update-editor-mode.html") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        clickAndSwitchAgain();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendMouseEvent({ type: "click" }, gFilteredSources.visibleItems[0].target);
}

function clickAndSwitchAgain() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    is(gFilteredSources.totalItems, 3,
      "The filtered sources view should have 3 items available.");
    is(gFilteredSources.visibleItems.length, 3,
      "The filtered sources view should have 3 items visible.");

    is(gFilteredSources.selectedValue,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedValue),
      "The correct item should be selected in the filtered sources view");
    is(gFilteredSources.selectedLabel,
       gDebugger.SourceUtils.trimUrlLength(gScripts.selectedLabel),
      "The correct item should be selected in the filtered sources view");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        switchFocusWithEscape();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendMouseEvent({ type: "click" }, gFilteredSources.visibleItems[2].target);
}

function switchFocusWithEscape() {
  window.addEventListener("popuphidden", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);

    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        focusAgainAfterEscape();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("ESCAPE");
}

function focusAgainAfterEscape() {
  window.addEventListener("popupshown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);

    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 1,
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
  window.addEventListener("popuphidden", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);

    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selectedValue;
    if (url.indexOf("test-script-switching-01.js") != -1) {

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line.");
        is(gScripts.visibleItems.length, 3,
          "Not all the correct scripts are shown after the search.");

        closeDebuggerAndFinish();
      });
    } else {
      ok(false, "How did you get here?");
    }
  });
  EventUtils.sendKey("RETURN");
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
    EventUtils.sendKey("BACK_SPACE")
  }
}

function append(text) {
  gSearchBox.focus();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i]);
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
  gScripts = null;
  gSearchBox = null;
  gFilteredSources = null;
  gMenulist = null;
});
