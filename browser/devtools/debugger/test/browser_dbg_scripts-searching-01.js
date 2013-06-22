/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests basic functionality of scripts filtering (token search and line jump).
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gEditor = null;
var gSources = null;
var gSearchBox = null;

function test()
{
  requestLongerTimeout(2);

  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);
      testScriptSearching();
    });
  });
}

function testScriptSearching() {
  let noMatchingSources = gDebugger.L10N.getStr("noMatchingSourcesText");
  let token = "";

  Services.tm.currentThread.dispatch({ run: function() {
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    write(":12");
    ok(gEditor.getCaretPosition().line == 11 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line.");

    EventUtils.synthesizeKey("g", { metaKey: true }, gDebugger);
    ok(gEditor.getCaretPosition().line == 12 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line after Meta+G");

    EventUtils.synthesizeKey("n", { ctrlKey: true }, gDebugger);
    ok(gEditor.getCaretPosition().line == 13 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line after Ctrl+N");

    EventUtils.synthesizeKey("G", { metaKey: true, shiftKey: true }, gDebugger);
    ok(gEditor.getCaretPosition().line == 12 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line after Meta+Shift+G");

    EventUtils.synthesizeKey("p", { ctrlKey: true }, gDebugger);
    ok(gEditor.getCaretPosition().line == 11 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line after Ctrl+P");

    for (let i = 0; i < 100; i++) {
      EventUtils.sendKey("DOWN", gDebugger);
    }
    ok(gEditor.getCaretPosition().line == 32 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line after multiple DOWN keys");

    for (let i = 0; i < 100; i++) {
      EventUtils.sendKey("UP", gDebugger);
    }
    ok(gEditor.getCaretPosition().line == 0 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line after multiple UP keys");


    token = "debugger";
    write("#" + token);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor didn't jump to the correct token. (1)");

    EventUtils.sendKey("DOWN", gDebugger);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (2)");

    EventUtils.sendKey("DOWN", gDebugger);
    ok(gEditor.getCaretPosition().line == 12 &&
       gEditor.getCaretPosition().col == 8 + token.length,
      "The editor didn't jump to the correct token. (3)");

    EventUtils.sendKey("RETURN", gDebugger);
    ok(gEditor.getCaretPosition().line == 19 &&
       gEditor.getCaretPosition().col == 4 + token.length,
      "The editor didn't jump to the correct token. (4)");

    EventUtils.sendKey("ENTER", gDebugger);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor didn't jump to the correct token. (5)");

    EventUtils.sendKey("UP", gDebugger);
    ok(gEditor.getCaretPosition().line == 19 &&
       gEditor.getCaretPosition().col == 4 + token.length,
      "The editor didn't jump to the correct token. (5.1)");


    token = "debugger;";
    write(":bogus#" + token);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (6)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write(":13#" + token);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (7)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write(":#" + token);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (8)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write("::#" + token);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (9)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write(":::#" + token);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (10)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");


    write("#" + token + ":bogus");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (6)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write("#" + token + ":13");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (7)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write("#" + token + ":");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (8)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write("#" + token + "::");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (9)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write("#" + token + ":::");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (10)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");


    write(":i am not a number");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't remain at the correct token. (11)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    write("#__i do not exist__");
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't remain at the correct token. (12)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");


    token = "debugger";
    write("#" + token);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor didn't jump to the correct token. (12.1)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    clear();
    EventUtils.sendKey("RETURN", gDebugger);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor shouldn't jump to another token. (12.2)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    EventUtils.sendKey("ENTER", gDebugger);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor shouldn't jump to another token. (12.3)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");


    write(":1:2:3:a:b:c:::12");
    ok(gEditor.getCaretPosition().line == 11 &&
       gEditor.getCaretPosition().col == 0,
      "The editor didn't jump to the correct line. (13)");

    write("#don't#find#me#instead#find#" + token);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor didn't jump to the correct token. (14)");

    EventUtils.sendKey("DOWN", gDebugger);
    ok(gEditor.getCaretPosition().line == 8 &&
       gEditor.getCaretPosition().col == 2 + token.length,
      "The editor didn't jump to the correct token. (15)");

    EventUtils.sendKey("DOWN", gDebugger);
    ok(gEditor.getCaretPosition().line == 12 &&
       gEditor.getCaretPosition().col == 8 + token.length,
      "The editor didn't jump to the correct token. (16)");

    EventUtils.sendKey("RETURN", gDebugger);
    ok(gEditor.getCaretPosition().line == 19 &&
       gEditor.getCaretPosition().col == 4 + token.length,
      "The editor didn't jump to the correct token. (17)");

    EventUtils.sendKey("ENTER", gDebugger);
    ok(gEditor.getCaretPosition().line == 2 &&
       gEditor.getCaretPosition().col == 44 + token.length,
      "The editor didn't jump to the correct token. (18)");

    EventUtils.sendKey("UP", gDebugger);
    ok(gEditor.getCaretPosition().line == 19 &&
       gEditor.getCaretPosition().col == 4 + token.length,
      "The editor didn't jump to the correct token. (18.1)");


    clear();
    ok(gEditor.getCaretPosition().line == 19 &&
       gEditor.getCaretPosition().col == 4 + token.length,
      "The editor didn't remain at the correct token. (19)");
    is(gSources.visibleItems.length, 1,
      "Not all the scripts are shown after the search. (20)");
    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice that matches are found.");

    closeDebuggerAndFinish();
  }}, 0);
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
});
