/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {}
Cu.import("resource:///modules/devtools/CssRuleView.jsm", tempScope);
let CssRuleView = tempScope.CssRuleView;
let _ElementStyle = tempScope._ElementStyle;
let _editableField = tempScope._editableField;
let inplaceEditor = tempScope._getInplaceEditorForSpan;

let doc;
let ruleDialog;
let ruleView;

function waitForEditorFocus(aParent, aCallback)
{
  aParent.addEventListener("focus", function onFocus(evt) {
    if (inplaceEditor(evt.target)) {
      aParent.removeEventListener("focus", onFocus, true);
      let editor = inplaceEditor(evt.target);
      executeSoon(function() {
        aCallback(editor);
      });
    }
  }, true);
}

function waitForEditorBlur(aEditor, aCallback)
{
  let input = aEditor.input;
  input.addEventListener("blur", function onBlur() {
    input.removeEventListener("blur", onBlur, false);
    executeSoon(function() {
      aCallback();
    });
  }, false);
}

var gRuleViewChanged = false;
function ruleViewChanged()
{
  gRuleViewChanged = true;
}

function expectChange()
{
  ok(gRuleViewChanged, "Rule view should have fired a change event.");
  gRuleViewChanged = false;
}

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

  ruleDialog = openDialog("chrome://browser/content/devtools/cssruleview.xul",
                          "cssruleviewtest",
                          "width=200,height=350");
  ruleDialog.addEventListener("load", function onLoad(evt) {
    ruleDialog.removeEventListener("load", onLoad);
    let doc = ruleDialog.document;
    ruleView = new CssRuleView(doc);
    doc.documentElement.appendChild(ruleView.element);
    ruleView.element.addEventListener("CssRuleViewChanged", ruleViewChanged, false);
    is(ruleView.element.querySelectorAll("#noResults").length, 1, "Has a no-results element.");
    ruleView.highlight(testElement);
    is(ruleView.element.querySelectorAll("#noResults").length, 0, "After a highlight, no longer has a no-results element.");
    ruleView.highlight(null);
    is(ruleView.element.querySelectorAll("#noResults").length, 1, "After highlighting null, has a no-results element again.");
    ruleView.highlight(testElement);

    waitForFocus(testCancelNew, ruleDialog);
  }, true);
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
      ok(!gRuleViewChanged, "Shouldn't get a change event after a cancel.");
      is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
      ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
      testCreateNew();
    });
    aEditor.input.blur();
  });

  EventUtils.synthesizeMouse(elementRuleEditor.closeBrace, 1, 1,
                             { },
                             ruleDialog);
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
      expectChange();
      is(elementRuleEditor.rule.textProps.length,  1, "Should have created a new text property.");
      is(elementRuleEditor.propertyList.children.length, 1, "Should have created a property editor.");
      let textProp = elementRuleEditor.rule.textProps[0];
      is(aEditor, inplaceEditor(textProp.editor.valueSpan), "Should be editing the value span now.");

      aEditor.input.value = "purple";
      waitForEditorBlur(aEditor, function() {
        expectChange();
        is(textProp.value, "purple", "Text prop should have been changed.");
        testEditProperty();
      });

      aEditor.input.blur();
    });
    EventUtils.sendKey("return", ruleDialog);
  });

  EventUtils.synthesizeMouse(elementRuleEditor.closeBrace, 1, 1,
                             { },
                             ruleDialog);
}

function testEditProperty()
{
  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;
  waitForEditorFocus(propEditor.element, function onNewElement(aEditor) {
    is(inplaceEditor(propEditor.nameSpan), aEditor, "Next focused editor should be the name editor.");
    let input = aEditor.input;
    waitForEditorFocus(propEditor.element, function onNewName(aEditor) {
      expectChange();
      input = aEditor.input;
      is(inplaceEditor(propEditor.valueSpan), aEditor, "Focus should have moved to the value.");

      waitForEditorBlur(aEditor, function() {
        expectChange();
        is(idRuleEditor.rule.style.getPropertyValue("border-color"), "red",
           "border-color should have been set.");

        let props = ruleView.element.querySelectorAll(".ruleview-property");
        for (let i = 0; i < props.length; i++) {
          is(props[i].hasAttribute("dirty"), i <= 1,
            "props[" + i + "] marked dirty as appropriate");
        }
        testDisableProperty();
      });

      for each (let ch in "red;") {
        EventUtils.sendChar(ch, ruleDialog);
      }
    });
    for each (let ch in "border-color:") {
      EventUtils.sendChar(ch, ruleDialog);
    }
  });

  EventUtils.synthesizeMouse(propEditor.nameSpan, 1, 1,
                             { },
                             ruleDialog);
}

function testDisableProperty()
{
  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  propEditor.enable.click();
  is(idRuleEditor.rule.style.getPropertyValue("border-color"), "", "Border-color should have been unset.");
  expectChange();

  propEditor.enable.click();
  is(idRuleEditor.rule.style.getPropertyValue("border-color"), "red",
     "Border-color should have been reset.");
  expectChange();

  finishTest();
}

function finishTest()
{
  ruleView.element.removeEventListener("CssRuleViewChanged", ruleViewChanged, false);
  ruleView.clear();
  ruleDialog.close();
  ruleDialog = ruleView = null;
  doc = null;
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
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,basic style inspector tests";
}
