/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let ruleWindow;
let ruleView;
let inspector;

function startTest()
{
  let style = '' +
    '#testid {' +
    '  background-color: blue;' +
    '} ' +
    '.testclass {' +
    '  background-color: green;' +
    '}';

  let styleNode = addStyle(doc, style);
  doc.body.innerHTML = '<div id="testid" class="testclass">Styled Node</div>';
  let testElement = doc.getElementById("testid");

  openRuleView((aInspector, aRuleView) => {
    inspector = aInspector;
    ruleView = aRuleView;
    ruleWindow = aRuleView.doc.defaultView;
    inspector.selection.setNode(testElement);
    inspector.once("inspector-updated", testCancelNew);
  });
}

function testCancelNew()
{
  // Start at the beginning: start to add a rule to the element's style
  // declaration, but leave it empty.
  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  waitForEditorFocus(elementRuleEditor.element, function onNewElement(aEditor) {
    is(inplaceEditor(elementRuleEditor.newPropSpan), aEditor, "Next focused editor should be the new property editor.");
    let input = aEditor.input;
    waitForEditorBlur(aEditor, function () {
      ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding request after a cancel.");
      is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
      ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
      testCreateNew();
    });
    aEditor.input.blur();
  });
  EventUtils.synthesizeMouse(elementRuleEditor.closeBrace, 1, 1,
                             { },
                             ruleWindow);
}

function testCreateNew()
{
  // Create a new property.
  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  waitForEditorFocus(elementRuleEditor.element, function onNewElement(aEditor) {
    is(inplaceEditor(elementRuleEditor.newPropSpan), aEditor, "Next focused editor should be the new property editor.");
    let input = aEditor.input;
    input.value = "background-color";

    waitForEditorFocus(elementRuleEditor.element, function onNewValue(aEditor) {
      promiseDone(expectRuleChange(elementRuleEditor.rule).then(() => {
        is(elementRuleEditor.rule.textProps.length,  1, "Should have created a new text property.");
        is(elementRuleEditor.propertyList.children.length, 1, "Should have created a property editor.");
        let textProp = elementRuleEditor.rule.textProps[0];
        is(aEditor, inplaceEditor(textProp.editor.valueSpan), "Should be editing the value span now.");
        aEditor.input.value = "#XYZ";
        waitForEditorBlur(aEditor, function() {
          promiseDone(expectRuleChange(elementRuleEditor.rule).then(() => {
            is(textProp.value, "#XYZ", "Text prop should have been changed.");
            is(textProp.editor._validate(), false, "#XYZ should not be a valid entry");
            testEditProperty();
          }));
        });
        aEditor.input.blur();
      }));
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, ruleWindow);
  });

  EventUtils.synthesizeMouse(elementRuleEditor.closeBrace, 1, 1,
                             { },
                             ruleWindow);
}

function testEditProperty()
{
  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;
  waitForEditorFocus(propEditor.element, function onNewElement(aEditor) {
    is(inplaceEditor(propEditor.nameSpan), aEditor, "Next focused editor should be the name editor.");
    let input = aEditor.input;
    waitForEditorFocus(propEditor.element, function onNewName(aEditor) {
      promiseDone(expectRuleChange(idRuleEditor.rule).then(() => {
        input = aEditor.input;
        is(inplaceEditor(propEditor.valueSpan), aEditor, "Focus should have moved to the value.");

        waitForEditorBlur(aEditor, function() {
          promiseDone(expectRuleChange(idRuleEditor.rule).then(() => {
            let value = idRuleEditor.rule.domRule._rawStyle().getPropertyValue("border-color");
            is(value, "red", "border-color should have been set.");
            is(propEditor._validate(), true, "red should be a valid entry");
            finishTest();
          }));
        });

        for (let ch of "red;") {
          EventUtils.sendChar(ch, ruleWindow);
        }
      }));
    });
    for (let ch of "border-color:") {
      EventUtils.sendChar(ch, ruleWindow);
    }
  });

  EventUtils.synthesizeMouse(propEditor.nameSpan, 32, 1,
                             { },
                             ruleWindow);
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

  content.location = "data:text/html,test rule view user changes";
}
