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
let brace;
// format :
//  [
//    what key to press,
//    modifers,
//    expected input box value after keypress,
//    selectedIndex of the popup,
//    total items in the popup
//  ]
let testData = [
  ["d", {}, "direction", 0, 3],
  ["VK_DOWN", {}, "display", 1, 3],
  ["VK_TAB", {}, "", -1, 10],
  ["n", {}, "none", -1, 0],
  ["VK_TAB", {shiftKey: true}, "display", -1, 0],
  ["VK_BACK_SPACE", {}, "", -1, 0],
  ["c", {}, "caption-side", 0, 10],
  ["o", {}, "color", 0, 6],
  ["VK_TAB", {}, "none", -1, 0],
  ["r", {}, "red", 0, 5],
  ["VK_DOWN", {}, "rgb", 1, 5],
  ["VK_DOWN", {}, "rgba", 2, 5],
  ["VK_DOWN", {}, "rosybrown", 3, 5],
  ["VK_DOWN", {}, "royalblue", 4, 5],
  ["VK_RIGHT", {}, "royalblue", -1, 0],
  [" ", {}, "royalblue !important", 0, 10],
  ["!", {}, "royalblue !important", 0, 0],
  ["VK_ESCAPE", {}, null, -1, 0]
];
function openRuleView() {
  var target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    inspector = toolbox.getCurrentPanel();
    inspector.sidebar.select("ruleview");

    // Highlight a node.
    let node = content.document.getElementsByTagName("h1")[0];
    inspector.selection.setNode(node);

    inspector.once("inspector-updated", testCompletion);
  });
}

function testCompletion() {
  ruleViewWindow = inspector.sidebar.getWindowForTab("ruleview");
  brace = ruleViewWindow.document.querySelector(".ruleview-ruleclose");

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

  let [key, modifiers] = testData[index];
  state = index;

  info("pressing key " + key + " to get result: [" + testData[index].slice(2) +
       "] for state " + state);
  if (/tab/ig.test(key)) {
    info("waiting for the editor to get focused");
    waitForEditorFocus(brace.parentNode, function onNewElement(aEditor) {
      info("editor focused : " + aEditor.input);
      editor = aEditor;
      checkState();
    });
  }
  else if (/(right|back_space|escape|return)/ig.test(key)) {
    info("added event listener for right|escape|back_space|return keys");
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
  EventUtils.synthesizeKey(key, modifiers, ruleViewWindow);
}

function checkState(event) {
  executeSoon(() => {
    info("After keypress for state " + state);
    let [key, modifier, completion, index, total] = testData[state];
    if (completion != null) {
      is(editor.input.value, completion,
         "Correct value is autocompleted for state " + state);
    }
    if (total == 0) {
      ok(!(editor.popup && editor.popup.isOpen), "Popup is closed for state " +
         state);
    }
    else {
      ok(editor.popup.isOpen, "Popup is open for state " + state);
      is(editor.popup.getItems().length, total,
         "Number of suggestions match for state " + state);
      is(editor.popup.selectedIndex, index,
         "Correct item is selected for state " + state);
    }
    checkStateAndMoveOn(state + 1);
  });
}

function finishUp() {
  brace = doc = inspector = editor = ruleViewWindow = state = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    doc.title = "Rule View Test";
    waitForFocus(openRuleView, content);
  }, true);

  content.location = "data:text/html,<h1 style='border: 1px solid red'>Filename:"+
                     " browser_bug894376_css_value_completion_new_property_value_pair.js</h1>";
}
