/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that CSS property names are autocompleted and cycled correctly.

const MAX_ENTRIES = 10;

let doc;
let inspector;
let ruleViewWindow;
let editor;
let state;
// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selectedIndex of the popup,
//    total items in the popup
//  ]
let testData = [
  ["VK_RIGHT", "border", -1, 0],
  ["-","border-bottom", 0, 10],
  ["b","border-bottom", 0, 6],
  ["VK_BACK_SPACE", "border-b", -1, 0],
  ["VK_BACK_SPACE", "border-", -1, 0],
  ["VK_BACK_SPACE", "border", -1, 0],
  ["VK_BACK_SPACE", "borde", -1, 0],
  ["VK_BACK_SPACE", "bord", -1, 0],
  ["VK_BACK_SPACE", "bor", -1, 0],
  ["VK_BACK_SPACE", "bo", -1, 0],
  ["VK_BACK_SPACE", "b", -1, 0],
  ["VK_BACK_SPACE", "", -1, 0],
  ["d", "direction", 0, 3],
  ["VK_DOWN", "display", 1, 3],
  ["VK_DOWN", "dominant-baseline", 2, 3],
  ["VK_DOWN", "direction", 0, 3],
  ["VK_DOWN", "display", 1, 3],
  ["VK_UP", "direction", 0, 3],
  ["VK_UP", "dominant-baseline", 2, 3],
  ["VK_UP", "display", 1, 3],
  ["VK_BACK_SPACE", "d", -1, 0],
  ["i", "direction", 0, 2],
  ["s", "display", -1, 0],
  ["VK_BACK_SPACE", "dis", -1, 0],
  ["VK_BACK_SPACE", "di", -1, 0],
  ["VK_BACK_SPACE", "d", -1, 0],
  ["VK_BACK_SPACE", "", -1, 0],
  ["f", "fill", 0, MAX_ENTRIES],
  ["i", "fill", 0, 4],
  ["VK_ESCAPE", null, -1, 0],
];

function openRuleView()
{
  var target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    inspector = toolbox.getCurrentPanel();
    inspector.sidebar.select("ruleview");

    // Highlight a node.
    let node = content.document.getElementsByTagName("h1")[0];
    inspector.selection.setNode(node);

    inspector.sidebar.once("ruleview-ready", testCompletion);
  });
}

function testCompletion()
{
  ruleViewWindow = inspector.sidebar.getWindowForTab("ruleview");
  let brace = ruleViewWindow.document.querySelector(".ruleview-propertyname");

  waitForEditorFocus(brace.parentNode, function onNewElement(aEditor) {
    editor = aEditor;
    checkStateAndMoveOn(0);
  });

  brace.click();
}

function checkStateAndMoveOn(index) {
  if (index == testData.length) {
    finishUp();
    return;
  }

  let [key] = testData[index];
  state = index;

  info("pressing key " + key + " to get result: [" + testData[index].slice(1) +
       "] for state " + state);
  if (/(right|back_space|escape)/ig.test(key)) {
    info("added event listener for right|back_space|escape keys");
    editor.input.addEventListener("keypress", function onKeypress() {
      if (editor.input) {
        editor.input.removeEventListener("keypress", onKeypress);
      }
      info("inside event listener");
      checkState();
    });
  }
  else {
    editor.once("after-suggest", checkState);
  }
  EventUtils.synthesizeKey(key, {}, ruleViewWindow);
}

function checkState() {
  executeSoon(() => {
    info("After keypress for state " + state);
    let [key, completion, index, total] = testData[state];
    if (completion != null) {
      is(editor.input.value, completion,
         "Correct value is autocompleted for state " + state);
    }
    if (total == 0) {
      ok(!(editor.popup && editor.popup.isOpen), "Popup is closed for state " +
         state);
    }
    else {
      ok(editor.popup._panel.state == "open" ||
         editor.popup._panel.state == "showing",
         "Popup is open for state " + state);
      is(editor.popup.getItems().length, total,
         "Number of suggestions match for state " + state);
      is(editor.popup.selectedIndex, index,
         "Correct item is selected for state " + state);
    }
    checkStateAndMoveOn(state + 1);
  });
}

function finishUp()
{
  doc = inspector = editor = ruleViewWindow = state = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    doc.title = "Rule View Test";
    waitForFocus(openRuleView, content);
  }, true);

  content.location = "data:text/html,<h1 style='border: 1px solid red'>Filename" +
                     ": browser_bug893965_css_property_completion_existing_property.js</h1>";
}
