/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test several types of rule-view property edition

const TEST_URI = `
  <style type="text/css">
  #testid {
    background-color: blue;
  }
  .testclass, .unmatched {
    background-color: green;
  }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <div id="testid2">Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  yield testEditProperty(inspector, view);
  yield testDisableProperty(inspector, view);
  yield testPropertyStillMarkedDirty(inspector, view);
});

function* testEditProperty(inspector, ruleView) {
  let idRule = getRuleViewRuleEditor(ruleView, 1).rule;
  let prop = idRule.textProps[0];

  let editor = yield focusEditableField(ruleView, prop.editor.nameSpan);
  let input = editor.input;
  is(inplaceEditor(prop.editor.nameSpan), editor,
    "Next focused editor should be the name editor.");

  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length,
    "Editor contents are selected.");

  // Try clicking on the editor's input again, shouldn't cause trouble
  // (see bug 761665).
  EventUtils.synthesizeMouse(input, 1, 1, {}, ruleView.styleWindow);
  input.select();

  info("Entering property name \"border-color\" followed by a colon to " +
    "focus the value");
  let onNameDone = ruleView.once("ruleview-changed");
  let onFocus = once(idRule.editor.element, "focus", true);
  EventUtils.sendString("border-color:", ruleView.styleWindow);
  yield onFocus;
  yield onNameDone;

  info("Verifying that the focused field is the valueSpan");
  editor = inplaceEditor(ruleView.styleDocument.activeElement);
  input = editor.input;
  is(inplaceEditor(prop.editor.valueSpan), editor,
    "Focus should have moved to the value.");
  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length,
    "Editor contents are selected.");

  info("Entering a value following by a semi-colon to commit it");
  let onBlur = once(editor.input, "blur");
  // Use sendChar() to pass each character as a string so that we can test
  // prop.editor.warning.hidden after each character.
  for (let ch of "red;") {
    let onPreviewDone = ruleView.once("ruleview-changed");
    EventUtils.sendChar(ch, ruleView.styleWindow);
    ruleView.debounce.flush();
    yield onPreviewDone;
    is(prop.editor.warning.hidden, true,
      "warning triangle is hidden or shown as appropriate");
  }
  yield onBlur;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "border-color"
  });
  is(newValue, "red", "border-color should have been set.");

  ruleView.styleDocument.activeElement.blur();
  yield addProperty(ruleView, 1, "color", "red", ";");

  let props = ruleView.element.querySelectorAll(".ruleview-property");
  for (let i = 0; i < props.length; i++) {
    is(props[i].hasAttribute("dirty"), i <= 1,
      "props[" + i + "] marked dirty as appropriate");
  }
}

function* testDisableProperty(inspector, ruleView) {
  let idRule = getRuleViewRuleEditor(ruleView, 1).rule;
  let prop = idRule.textProps[0];

  info("Disabling a property");
  yield togglePropStatus(ruleView, prop);

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "border-color"
  });
  is(newValue, "", "Border-color should have been unset.");

  info("Enabling the property again");
  yield togglePropStatus(ruleView, prop);

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "border-color"
  });
  is(newValue, "red", "Border-color should have been reset.");
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
