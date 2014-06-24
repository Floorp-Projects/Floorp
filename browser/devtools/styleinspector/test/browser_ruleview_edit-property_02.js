/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test several types of rule-view property edition

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_ui.js");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test document");
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
  yield testEditProperty(inspector, view);
  yield testDisableProperty(inspector, view);
  yield testPropertyStillMarkedDirty(inspector, view);
});

function* testEditProperty(inspector, ruleView) {
  let idRuleEditor = getRuleViewRuleEditor(ruleView, 1);
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
    is(props[i].hasAttribute("dirty"), i <= 0,
      "props[" + i + "] marked dirty as appropriate");
  }
}

function* testDisableProperty(inspector, ruleView) {
  let idRuleEditor = getRuleViewRuleEditor(ruleView, 1);
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
    is(props[i].hasAttribute("dirty"), i <= 0,
      "props[" + i + "] marked dirty as appropriate");
  }
}
