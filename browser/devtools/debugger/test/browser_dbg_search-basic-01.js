/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests basic search functionality (find token and jump to line).
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gPanel, gDebugger;
let gEditor, gSources, gFiltering, gSearchBox;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFiltering = gDebugger.DebuggerView.Filtering;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceShown(gPanel, ".html").then(performTest);
  });
}

function performTest() {
  setText(gSearchBox, "#html");
  
  EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "html"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 35, 7),
    "The editor didn't jump to the correct line.");

  EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "html"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 5, 6),
    "The editor didn't jump to the correct line.");

  EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "html"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 3, 15),
    "The editor didn't jump to the correct line.");
  
  setText(gSearchBox, ":12");
  is(gFiltering.searchData.toSource(), '[":", ["", 12]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 12),
    "The editor didn't jump to the correct line.");

  EventUtils.synthesizeKey("g", { metaKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '[":", ["", 13]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 13),
    "The editor didn't jump to the correct line after Meta+G.");

  EventUtils.synthesizeKey("n", { ctrlKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '[":", ["", 14]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14),
    "The editor didn't jump to the correct line after Ctrl+N.");

  EventUtils.synthesizeKey("G", { metaKey: true, shiftKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '[":", ["", 13]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 13),
    "The editor didn't jump to the correct line after Meta+Shift+G.");

  EventUtils.synthesizeKey("p", { ctrlKey: true }, gDebugger);
  is(gFiltering.searchData.toSource(), '[":", ["", 12]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 12),
    "The editor didn't jump to the correct line after Ctrl+P.");

  for (let i = 0; i < 100; i++) {
    EventUtils.sendKey("DOWN", gDebugger);
  }
  is(gFiltering.searchData.toSource(), '[":", ["", 36]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 36),
    "The editor didn't jump to the correct line after multiple DOWN keys.");

  for (let i = 0; i < 100; i++) {
    EventUtils.sendKey("UP", gDebugger);
  }
  is(gFiltering.searchData.toSource(), '[":", ["", 1]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 1),
    "The editor didn't jump to the correct line after multiple UP keys.");


  let token = "debugger";
  setText(gSearchBox, "#" + token);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor didn't jump to the correct token (1).");

  EventUtils.sendKey("DOWN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length),
    "The editor didn't jump to the correct token (2).");

  EventUtils.sendKey("DOWN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 18, 15 + token.length),
    "The editor didn't jump to the correct token (3).");

  EventUtils.sendKey("RETURN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 26, 11 + token.length),
    "The editor didn't jump to the correct token (4).");

  EventUtils.sendKey("RETURN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor didn't jump to the correct token (5).");

  EventUtils.sendKey("UP", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 26, 11 + token.length),
    "The editor didn't jump to the correct token (6).");

  setText(gSearchBox, ":bogus#" + token + ";");
  is(gFiltering.searchData.toSource(), '["#", [":bogus", "debugger;"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (7).");

  setText(gSearchBox, ":13#" + token + ";");
  is(gFiltering.searchData.toSource(), '["#", [":13", "debugger;"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (8).");

  setText(gSearchBox, ":#" + token + ";");
  is(gFiltering.searchData.toSource(), '["#", [":", "debugger;"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (9).");

  setText(gSearchBox, "::#" + token + ";");
  is(gFiltering.searchData.toSource(), '["#", ["::", "debugger;"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (10).");

  setText(gSearchBox, ":::#" + token + ";");
  is(gFiltering.searchData.toSource(), '["#", [":::", "debugger;"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (11).");


  setText(gSearchBox, "#" + token + ";" + ":bogus");
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger;:bogus"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (12).");

  setText(gSearchBox, "#" + token + ";" + ":13");
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger;:13"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (13).");

  setText(gSearchBox, "#" + token + ";" + ":");
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger;:"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (14).");

  setText(gSearchBox, "#" + token + ";" + "::");
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger;::"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (15).");

  setText(gSearchBox, "#" + token + ";" + ":::");
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger;:::"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't jump to the correct token (16).");


  setText(gSearchBox, ":i am not a number");
  is(gFiltering.searchData.toSource(), '[":", ["", 0]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't remain at the correct token (17).");

  setText(gSearchBox, "#__i do not exist__");
  is(gFiltering.searchData.toSource(), '["#", ["", "__i do not exist__"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length + 1),
    "The editor didn't remain at the correct token (18).");


  setText(gSearchBox, "#" + token);
  is(gFiltering.searchData.toSource(), '["#", ["", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor didn't jump to the correct token (19).");


  clearText(gSearchBox);
  is(gFiltering.searchData.toSource(), '["", [""]]',
    "The searchbox data wasn't parsed correctly.");

  EventUtils.sendKey("RETURN", gDebugger);
  is(gFiltering.searchData.toSource(), '["", [""]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor shouldn't jump to another token (20).");

  EventUtils.sendKey("RETURN", gDebugger);
  is(gFiltering.searchData.toSource(), '["", [""]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor shouldn't jump to another token (21).");


  setText(gSearchBox, ":1:2:3:a:b:c:::12");
  is(gFiltering.searchData.toSource(), '[":", [":1:2:3:a:b:c::", 12]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 12),
    "The editor didn't jump to the correct line (22).");

  setText(gSearchBox, "#don't#find#me#instead#find#" + token);
  is(gFiltering.searchData.toSource(), '["#", ["#don\'t#find#me#instead#find", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor didn't jump to the correct token (23).");

  EventUtils.sendKey("DOWN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["#don\'t#find#me#instead#find", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 14, 9 + token.length),
    "The editor didn't jump to the correct token (24).");

  EventUtils.sendKey("DOWN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["#don\'t#find#me#instead#find", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 18, 15 + token.length),
    "The editor didn't jump to the correct token (25).");

  EventUtils.sendKey("RETURN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["#don\'t#find#me#instead#find", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 26, 11 + token.length),
    "The editor didn't jump to the correct token (26).");

  EventUtils.sendKey("RETURN", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["#don\'t#find#me#instead#find", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 8, 12 + token.length),
    "The editor didn't jump to the correct token (27).");

  EventUtils.sendKey("UP", gDebugger);
  is(gFiltering.searchData.toSource(), '["#", ["#don\'t#find#me#instead#find", "debugger"]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 26, 11 + token.length),
    "The editor didn't jump to the correct token (28).");


  clearText(gSearchBox);
  is(gFiltering.searchData.toSource(), '["", [""]]',
    "The searchbox data wasn't parsed correctly.");
  ok(isCaretPos(gPanel, 26, 11 + token.length),
    "The editor didn't remain at the correct token (29).");
  is(gSources.visibleItems.length, 1,
    "Not all the sources are shown after the search (30).");


  gEditor.focus();
  gEditor.setSelection.apply(gEditor, gEditor.getPosition(1, 5));
  ok(isCaretPos(gPanel, 1, 6),
    "The editor caret position didn't update after selecting some text.");

  EventUtils.synthesizeKey("F", { accelKey: true });
  is(gFiltering.searchData.toSource(), '["#", ["", "!-- "]]',
    "The searchbox data wasn't parsed correctly.");
  is(gSearchBox.value, "#!-- ",
    "The search field has the right initial value (1).");

  gEditor.focus();
  gEditor.setSelection.apply(gEditor, gEditor.getPosition(415, 418));
  ok(isCaretPos(gPanel, 21, 30),
    "The editor caret position didn't update after selecting some number.");

  EventUtils.synthesizeKey("L", { accelKey: true });
  is(gFiltering.searchData.toSource(), '[":", ["", 100]]',
    "The searchbox data wasn't parsed correctly.");
  is(gSearchBox.value, ":100",
    "The search field has the right initial value (2).");


  closeDebuggerAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFiltering = null;
  gSearchBox = null;
});
