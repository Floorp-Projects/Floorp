/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let inspector;
let ruleWindow;
let ruleView;

function startTest(aInspector, aRuleView)
{
  inspector = aInspector;
  ruleWindow = aRuleView.doc.defaultView;
  ruleView = aRuleView;
  let style = "" +
    "#testid {" +
    "  background-color: blue;" +
    "}" +
    ".testclass, .unmatched {" +
    "  background-color: green;" +
    "}";

  let styleNode = addStyle(doc, style);
  doc.body.innerHTML = "<div id='testid' class='testclass'>Styled Node</div>" +
                       "<div id='testid2'>Styled Node</div>";

  let testElement = doc.getElementById("testid");
  inspector.selection.setNode(testElement);
  inspector.once("inspector-updated", () => {
    is(ruleView.element.querySelectorAll("#noResults").length, 0, "After a highlight, no longer has a no-results element.");
    inspector.selection.setNode(null);
    inspector.once("inspector-updated", () => {
      is(ruleView.element.querySelectorAll("#noResults").length, 1, "After highlighting null, has a no-results element again.");
      inspector.selection.setNode(testElement);
      inspector.once("inspector-updated", () => {
        let classEditor = ruleView.element.children[2]._ruleEditor;
        is(classEditor.selectorText.querySelector(".ruleview-selector-matched").textContent, ".testclass", ".textclass should be matched.");
        is(classEditor.selectorText.querySelector(".ruleview-selector-unmatched").textContent, ".unmatched", ".unmatched should not be matched.");

        testCancelNew();
      });
    });
  });
}

function testCancelNew()
{
  // Start at the beginning: start to add a rule to the element's style
  // declaration, but leave it empty.

  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  waitForEditorFocus(elementRuleEditor.element, function onNewElement(aEditor) {
    is(inplaceEditor(elementRuleEditor.newPropSpan), aEditor, "Next focused editor should be the new property editor.");
    waitForEditorBlur(aEditor, function () {
      ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding modification request after a cancel.");
      is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
      ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
      testCancelNewOnEscape();
    });
    aEditor.input.blur();
  });

  EventUtils.synthesizeMouse(elementRuleEditor.closeBrace, 1, 1,
                             { },
                             ruleWindow);
}

function testCancelNewOnEscape()
{
  // Start at the beginning: start to add a rule to the element's style
  // declaration, add some text, then press escape.

  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  waitForEditorFocus(elementRuleEditor.element, function onNewElement(aEditor) {
    is(inplaceEditor(elementRuleEditor.newPropSpan), aEditor, "Next focused editor should be the new property editor.");
    for (let ch of "background") {
      EventUtils.sendChar(ch, ruleWindow);
    }
    waitForEditorBlur(aEditor, function () {
      ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding modification request after a cancel.");
      is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
      ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
      testCreateNew();
    });
    EventUtils.synthesizeKey("VK_ESCAPE", { });
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

    ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

    // Try clicking on the editor's input again, shouldn't cause trouble (see bug 761665).
    EventUtils.synthesizeMouse(input, 1, 1, { }, ruleWindow);
    input.select();

    input.value = "background-color";

    waitForEditorFocus(elementRuleEditor.element, function onNewValue(aEditor) {
      promiseDone(expectRuleChange(elementRuleEditor.rule).then(() => {
        is(elementRuleEditor.rule.textProps.length,  1, "Should have created a new text property.");
        is(elementRuleEditor.propertyList.children.length, 1, "Should have created a property editor.");
        let textProp = elementRuleEditor.rule.textProps[0];
        is(aEditor, inplaceEditor(textProp.editor.valueSpan), "Should be editing the value span now.");

        aEditor.input.value = "purple";
        waitForEditorBlur(aEditor, function() {
          expectRuleChange(elementRuleEditor.rule).then(() => {
            is(textProp.value, "purple", "Text prop should have been changed.");
            testEditProperty();
          });
        });

        aEditor.input.blur();
      }));
    });
    EventUtils.sendKey("return", ruleWindow);
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

    dump("SELECTION END IS: " + input.selectionEnd + "\n");
    ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

    // Try clicking on the editor's input again, shouldn't cause trouble (see bug 761665).
    EventUtils.synthesizeMouse(input, 1, 1, { }, ruleWindow);
    input.select();

    waitForEditorFocus(propEditor.element, function onNewName(aEditor) {
      promiseDone(expectRuleChange(idRuleEditor.rule).then(() => {
        is(inplaceEditor(propEditor.valueSpan), aEditor, "Focus should have moved to the value.");

        input = aEditor.input;

        ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

        // Try clicking on the editor's input again, shouldn't cause trouble (see bug 761665).
        EventUtils.synthesizeMouse(input, 1, 1, { }, ruleWindow);
        input.select();

        waitForEditorBlur(aEditor, function() {
          promiseDone(expectRuleChange(idRuleEditor.rule).then(() => {
            is(idRuleEditor.rule.style._rawStyle().getPropertyValue("border-color"), "red",
               "border-color should have been set.");

            let props = ruleView.element.querySelectorAll(".ruleview-property");
            for (let i = 0; i < props.length; i++) {
              is(props[i].hasAttribute("dirty"), i <= 1,
                "props[" + i + "] marked dirty as appropriate");
            }
            testDisableProperty();
          }));
        });

        for (let ch of "red;") {
          EventUtils.sendChar(ch, ruleWindow);
          is(propEditor.warning.hidden, true,
            "warning triangle is hidden or shown as appropriate");
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

function testDisableProperty()
{
  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  propEditor.enable.click();
  promiseDone(expectRuleChange(idRuleEditor.rule).then(() => {
    is(idRuleEditor.rule.style._rawStyle().getPropertyValue("border-color"), "", "Border-color should have been unset.");

    propEditor.enable.click();
    return expectRuleChange(idRuleEditor.rule);
  }).then(() => {
    is(idRuleEditor.rule.style._rawStyle().getPropertyValue("border-color"), "red",
      "Border-color should have been reset.");

    testPropertyStillMarkedDirty();
  }));
}

function testPropertyStillMarkedDirty() {
  // Select an unstyled node.
  let testElement = doc.getElementById("testid2");
  inspector.selection.setNode(testElement);
  inspector.once("inspector-updated", () => {
    // Select the original node again.
    testElement = doc.getElementById("testid");
    inspector.selection.setNode(testElement);
    inspector.once("inspector-updated", () => {
      let props = ruleView.element.querySelectorAll(".ruleview-property");
      for (let i = 0; i < props.length; i++) {
        is(props[i].hasAttribute("dirty"), i <= 1,
          "props[" + i + "] marked dirty as appropriate");
      }
      finishTest();
    });
  });
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
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    waitForFocus(() => openRuleView(startTest), content);
  }, true);

  content.location = "data:text/html,basic style inspector tests";
}
