/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that changes are previewed when editing a property value

let doc;
let testElement;
let ruleWindow;
let ruleView;
let inspector;

// Format
// {
//   value : what to type in the field
//   expected : expected computed style on the targeted element
// }
let testData = [
  {value: "inline", expected: "inline"},
  {value: "inline-block", expected: "inline-block"},

  // Invalid property values should not apply, and should fall back to default
  {value: "red", expected: "block"},
  {value: "something", expected: "block"},

  {escape: true, value: "inline", expected: "block"},
  {escape: true, value: "block", expected: "block"}
];

function startTest()
{
  let style = '#testid {display:block;}';

  let styleNode = addStyle(doc, style);
  doc.body.innerHTML = '<div id="testid">Styled Node</div><span>inline element</span>';
  testElement = doc.getElementById("testid");

  openRuleView((aInspector, aRuleView) => {
    inspector = aInspector;
    ruleView = aRuleView;
    ruleWindow = aRuleView.doc.defaultView;
    inspector.selection.setNode(testElement);
    inspector.once("inspector-updated", () => loopTestData(0));
  });
}

function loopTestData(index)
{
  if(index === testData.length) {
    finishTest();
    return;
  }

  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;
  waitForEditorFocus(propEditor.element, function(aEditor) {
    is(inplaceEditor(propEditor.valueSpan), aEditor, "Focused editor should be the value.");

    let thisTest = testData[index];

    // Entering a correct value for the property
    for (let ch of thisTest.value) {
      EventUtils.sendChar(ch, ruleWindow);
    }
    if (thisTest.escape) {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    } else {
      EventUtils.synthesizeKey("VK_RETURN", {});
    }

    // While the editor is still focused in, the display should have changed already
    executeSoon(() => {
      is(content.getComputedStyle(testElement).display,
        testData[index].expected,
        "Element should be previewed as " + testData[index].expected);

      loopTestData(index + 1);
    });
  });

  EventUtils.synthesizeMouse(propEditor.valueSpan, 1, 1, {}, ruleWindow);
}

function finishTest()
{
  inspector = ruleWindow = ruleView = null;
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function changedValues_load(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, changedValues_load, true);
    doc = content.document;
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,test rule view live preview on user changes";
}
