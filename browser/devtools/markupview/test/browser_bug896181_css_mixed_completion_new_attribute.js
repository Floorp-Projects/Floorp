/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test CSS state is correctly determined and the corresponding suggestions are
// displayed. i.e. CSS property suggestions are shown when cursor is like:
// ```style="di|"``` where | is teh cursor; And CSS value suggestion is
// displayed when the cursor is like: ```style="display:n|"``` properly. No
// suggestions should ever appear when the attribute is not a style attribute.
// The correctness and cycling of the suggestions is covered in the ruleview
// tests.

function test() {
  let inspector;
  let {
    getInplaceEditorForSpan: inplaceEditor
  } = devtools.require("devtools/shared/inplace-editor");

  waitForExplicitFinish();

  // Will hold the doc we're viewing
  let doc;

  // Holds the MarkupTool object we're testing.
  let markup;
  let editor;
  let state;
  // format :
  //  [
  //    what key to press,
  //    expected input box value after keypress,
  //    expected input.selectionStart,
  //    expected input.selectionEnd,
  //    is popup expected to be open ?
  //  ]
  let testData = [
    ['s', 's', 1, 1, false],
    ['t', 'st', 2, 2, false],
    ['y', 'sty', 3, 3, false],
    ['l', 'styl', 4, 4, false],
    ['e', 'style', 5, 5, false],
    ['=', 'style=', 6, 6, false],
    ['"', 'style="', 7, 7, false],
    ['d', 'style="direction', 8, 16, true],
    ['VK_DOWN', 'style="display', 8, 14, true],
    ['VK_RIGHT', 'style="display', 14, 14, false],
    [':', 'style="display:', 15, 15, false],
    ['n', 'style="display:none', 16, 19, false],
    ['VK_BACK_SPACE', 'style="display:n', 16, 16, false],
    ['VK_BACK_SPACE', 'style="display:', 15, 15, false],
    [' ', 'style="display: ', 16, 16, false],
    [' ', 'style="display:  ', 17, 17, false],
    ['i', 'style="display:  inherit', 18, 24, true],
    ['VK_RIGHT', 'style="display:  inherit', 24, 24, false],
    [';', 'style="display:  inherit;', 25, 25, false],
    [' ', 'style="display:  inherit; ', 26, 26, false],
    [' ', 'style="display:  inherit;  ', 27, 27, false],
    ['VK_LEFT', 'style="display:  inherit;  ', 26, 26, false],
    ['c', 'style="display:  inherit; caption-side ', 27, 38, true],
    ['o', 'style="display:  inherit; color ', 28, 31, true],
    ['VK_RIGHT', 'style="display:  inherit; color ', 31, 31, false],
    [' ', 'style="display:  inherit; color  ', 32, 32, false],
    ['c', 'style="display:  inherit; color c ', 33, 33, false],
    ['VK_BACK_SPACE', 'style="display:  inherit; color  ', 32, 32, false],
    [':', 'style="display:  inherit; color : ', 33, 33, false],
    ['c', 'style="display:  inherit; color :cadetblue ', 34, 42, true],
    ['VK_DOWN', 'style="display:  inherit; color :chartreuse ', 34, 43, true],
    ['VK_RETURN', 'style="display:  inherit; color :chartreuse"', -1, -1, false]
  ];

  function startTests() {
    markup = inspector.markup;
    markup.expandAll().then(() => {
      let node = getContainerForRawNode(markup, doc.querySelector("#node14")).editor;
      let attr = node.newAttr;
      attr.focus();
      EventUtils.sendKey("return", inspector.panelWin);
      editor = inplaceEditor(attr);
      checkStateAndMoveOn(0);
    });
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
    if (/(down|left|right|back_space|return)/ig.test(key)) {
      info("added event listener for down|left|right|back_space|return keys");
      editor.input.addEventListener("keypress", function onKeypress() {
        if (editor.input) {
          editor.input.removeEventListener("keypress", onKeypress);
        }
        info("inside event listener");
        checkState();
      }) 
    }
    else {
      editor.once("after-suggest", checkState);
    }
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
  }

  function checkState() {
    executeSoon(() => {
      info("After keypress for state " + state);
      let [key, completion, selStart, selEnd, popupOpen] = testData[state];
      if (selEnd != -1) {
        is(editor.input.value, completion,
           "Correct value is autocompleted for state " + state);
        is(editor.input.selectionStart, selStart,
           "Selection is starting at the right location for state " + state);
        is(editor.input.selectionEnd, selEnd,
           "Selection is ending at the right location for state " + state);
        if (popupOpen) {
          ok(editor.popup._panel.state == "open" ||
             editor.popup._panel.state == "showing",
             "Popup is open for state " + state);
        }
        else {
          ok(editor.popup._panel.state != "open" &&
             editor.popup._panel.state != "showing",
             "Popup is closed for state " + state);
        }
      }
      else {
        let editor = getContainerForRawNode(markup, doc.querySelector("#node14")).editor;
        let attr = editor.attrs["style"].querySelector(".editable");
        is(attr.textContent, completion,
           "Correct value is persisted after pressing Enter for state " + state);
      }
      checkStateAndMoveOn(state + 1);
    });
  }

  // Create the helper tab for parsing...
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);
  content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_edit.html";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      startTests();
    });
  }

  function finishUp() {
    while (markup.undo.canUndo()) {
      markup.undo.undo();
    }
    doc = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
