/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test all sorts of additions and updates of properties in the rule-view
// FIXME: TO BE SPLIT IN *MANY* SMALLER TESTS

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_ui.js");
  let {toolbox, inspector, view} = yield openRuleView();

  yield testContentAfterNodeSelection(inspector, view);
  yield testCancelNew(inspector, view);
  yield testCancelNewOnEscape(inspector, view);
  yield testCreateNew(inspector, view);
  yield testEditProperty(inspector, view);
  yield testDisableProperty(inspector, view);
  yield testPropertyStillMarkedDirty(inspector, view);
});

function* testContentAfterNodeSelection(inspector, ruleView) {
  let style = "" +
    "#testid {" +
    "  background-color: blue;" +
    "}" +
    ".testclass, .unmatched {" +
    "  background-color: green;" +
    "}";

  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = "<div id='testid' class='testclass'>Styled Node</div>" +
                                    "<div id='testid2'>Styled Node</div>";

  yield selectNode("#testid", inspector);
  is(ruleView.element.querySelectorAll("#noResults").length, 0,
    "After a highlight, no longer has a no-results element.");

  yield clearCurrentNodeSelection(inspector)
  is(ruleView.element.querySelectorAll("#noResults").length, 1,
    "After highlighting null, has a no-results element again.");

  yield selectNode("#testid", inspector);
  let classEditor = ruleView.element.children[2]._ruleEditor;
  is(classEditor.selectorText.querySelector(".ruleview-selector-matched").textContent,
    ".testclass", ".textclass should be matched.");
  is(classEditor.selectorText.querySelector(".ruleview-selector-unmatched").textContent,
    ".unmatched", ".unmatched should not be matched.");
}

function* testCancelNew(inspector, ruleView) {
  // Start at the beginning: start to add a rule to the element's style
  // declaration, but leave it empty.

  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "Property editor is focused");

  let onBlur = once(editor.input, "blur");
  editor.input.blur();
  yield onBlur;

  ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding modification request after a cancel.");
  is(elementRuleEditor.rule.textProps.length, 0, "Should have canceled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
}

function* testCancelNewOnEscape(inspector, ruleView) {
  // Start at the beginning: start to add a rule to the element's style
  // declaration, add some text, then press escape.

  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "Next focused editor should be the new property editor.");
  for (let ch of "background") {
    EventUtils.sendChar(ch, ruleView.doc.defaultView);
  }

  let onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield onBlur;

  ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding modification request after a cancel.");
  is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
}

function* testCreateNew(inspector, ruleView) {
  // Create a new property.
  let elementRuleEditor = ruleView.element.children[0]._ruleEditor;
  let editor = yield focusEditableField(elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "Next focused editor should be the new property editor.");

  let input = editor.input;

  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length,
    "Editor contents are selected.");

  // Try clicking on the editor's input again, shouldn't cause trouble (see bug 761665).
  EventUtils.synthesizeMouse(input, 1, 1, {}, ruleView.doc.defaultView);
  input.select();

  info("Entering the property name");
  input.value = "background-color";

  info("Pressing RETURN and waiting for the value field focus");
  let onFocus = once(elementRuleEditor.element, "focus", true);
  EventUtils.sendKey("return", ruleView.doc.defaultView);
  yield onFocus;
  yield elementRuleEditor.rule._applyingModifications;

  editor = inplaceEditor(ruleView.doc.activeElement);

  is(elementRuleEditor.rule.textProps.length,  1, "Should have created a new text property.");
  is(elementRuleEditor.propertyList.children.length, 1, "Should have created a property editor.");
  let textProp = elementRuleEditor.rule.textProps[0];
  is(editor, inplaceEditor(textProp.editor.valueSpan), "Should be editing the value span now.");

  editor.input.value = "purple";
  let onBlur = once(editor.input, "blur");
  editor.input.blur();
  yield onBlur;
  yield elementRuleEditor.rule._applyingModifications;

  is(textProp.value, "purple", "Text prop should have been changed.");
}

function* testEditProperty(inspector, ruleView) {
  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  let editor = yield focusEditableField(propEditor.nameSpan);
  let input = editor.input;
  is(inplaceEditor(propEditor.nameSpan), editor, "Next focused editor should be the name editor.");

  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

  // Try clicking on the editor's input again, shouldn't cause trouble (see bug 761665).
  EventUtils.synthesizeMouse(input, 1, 1, {}, ruleView.doc.defaultView);
  input.select();

  info("Entering property name followed by a colon to focus the value");
  let onFocus = once(idRuleEditor.element, "focus", true);
  for (let ch of "border-color:") {
    EventUtils.sendChar(ch, ruleView.doc.defaultView);
  }
  yield onFocus;
  yield idRuleEditor.rule._applyingModifications;

  info("Verifying that the focused field is the valueSpan");
  editor = inplaceEditor(ruleView.doc.activeElement);
  input = editor.input;
  is(inplaceEditor(propEditor.valueSpan), editor, "Focus should have moved to the value.");
  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

  info("Entering a value following by a semi-colon to commit it");
  let onBlur = once(editor.input, "blur");
  for (let ch of "red;") {
    EventUtils.sendChar(ch, ruleView.doc.defaultView);
    is(propEditor.warning.hidden, true,
      "warning triangle is hidden or shown as appropriate");
  }
  yield onBlur;
  yield idRuleEditor.rule._applyingModifications;

  is(idRuleEditor.rule.style._rawStyle().getPropertyValue("border-color"), "red",
     "border-color should have been set.");

  let props = ruleView.element.querySelectorAll(".ruleview-property");
  for (let i = 0; i < props.length; i++) {
    is(props[i].hasAttribute("dirty"), i <= 1,
      "props[" + i + "] marked dirty as appropriate");
  }
}

function* testDisableProperty(inspector, ruleView) {
  let idRuleEditor = ruleView.element.children[1]._ruleEditor;
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  info("Disabling a property");
  propEditor.enable.click();
  yield idRuleEditor.rule._applyingModifications;
  is(idRuleEditor.rule.style._rawStyle().getPropertyValue("border-color"), "",
    "Border-color should have been unset.");

  info("Enabling the property again");
  propEditor.enable.click();
  yield idRuleEditor.rule._applyingModifications;
  is(idRuleEditor.rule.style._rawStyle().getPropertyValue("border-color"), "red",
    "Border-color should have been reset.");
}

function* testPropertyStillMarkedDirty(inspector, ruleView) {
  // Select an unstyled node.
  yield selectNode("#testid2", inspector);

  // Select the original node again.
  yield selectNode("#testid", inspector);

  let props = ruleView.element.querySelectorAll(".ruleview-property");
  for (let i = 0; i < props.length; i++) {
    is(props[i].hasAttribute("dirty"), i <= 1,
      "props[" + i + "] marked dirty as appropriate");
  }
}
