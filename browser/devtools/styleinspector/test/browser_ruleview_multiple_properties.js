/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors

let test = asyncTest(function*() {
  yield addTab("data:text/html,test rule view user changes");
  content.document.body.innerHTML = "<h1>Testing Multiple Properties</h1>";

  let {toolbox, inspector, view} = yield openRuleView();

  yield testCreateNewMulti(inspector, view);
  yield testCreateNewMultiDuplicates(inspector, view);
  yield testCreateNewMultiPriority(inspector, view);
  yield testCreateNewMultiUnfinished(inspector, view);
  yield testCreateNewMultiPartialUnfinished(inspector, view);
  yield testMultiValues(inspector, view);
});

/**
 * Create a new test element, select it, and return the rule-view ruleEditor
 */
function* createAndSelectNewElement(inspector, view) {
  info("Creating a new test element");
  let newElement = content.document.createElement("div");
  newElement.textContent = "Test Element";
  content.document.body.appendChild(newElement);

  info("Selecting the new element and waiting for the inspector to update");
  yield selectNode(newElement, inspector);

  info("Getting the rule-view rule editor for that new element");
  return view.element.children[0]._ruleEditor;
}

/**
 * Begin the creation of a new property, resolving after the editor
 * has been created.
 */
function* focusNewProperty(ruleEditor) {
  info("Clicking on the close ruleEditor brace to start edition");
  ruleEditor.closeBrace.scrollIntoView();
  let editor = yield focusEditableField(ruleEditor.closeBrace);

  is(inplaceEditor(ruleEditor.newPropSpan), editor, "Focused editor is the new property editor.");
  is(ruleEditor.rule.textProps.length,  0, "Starting with one new text property.");
  is(ruleEditor.propertyList.children.length, 1, "Starting with two property editors.");

  return editor;
}

/**
 * Fully create a new property, given some text input
 */
function* createNewProperty(ruleEditor, inputValue) {
  info("Creating a new property editor");
  let editor = yield focusNewProperty(ruleEditor);

  info("Entering the value " + inputValue);
  editor.input.value = inputValue;

  info("Submitting the new value and waiting for value field focus");
  let onFocus = once(ruleEditor.element, "focus", true);
  EventUtils.synthesizeKey("VK_RETURN", {}, ruleEditor.element.ownerDocument.defaultView);
  yield onFocus;
}

function* testCreateNewMulti(inspector, view) {
  let ruleEditor = yield createAndSelectNewElement(inspector, view);
  yield createNewProperty(ruleEditor,
    "color:blue;background : orange   ; text-align:center; border-color: green;");

  is(ruleEditor.rule.textProps.length, 4, "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 5, "Should have created a new property editor.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "blue", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "background", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "text-align", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "center", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "border-color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "green", "Should have correct property value");

  yield inspector.once("inspector-updated");
}

function* testCreateNewMultiDuplicates(inspector, view) {
  let ruleEditor = yield createAndSelectNewElement(inspector, view);
  yield createNewProperty(ruleEditor,
    "color:red;color:orange;color:yellow;color:green;color:blue;color:indigo;color:violet;");

  is(ruleEditor.rule.textProps.length, 7, "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 8, "Should have created new property editors.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "red", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "yellow", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "green", "Should have correct property value");

  is(ruleEditor.rule.textProps[4].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[4].value, "blue", "Should have correct property value");

  is(ruleEditor.rule.textProps[5].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[5].value, "indigo", "Should have correct property value");

  is(ruleEditor.rule.textProps[6].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[6].value, "violet", "Should have correct property value");

  yield inspector.once("inspector-updated");
}

function* testCreateNewMultiPriority(inspector, view) {
  let ruleEditor = yield createAndSelectNewElement(inspector, view);
  yield createNewProperty(ruleEditor,
    "color:red;width:100px;height: 100px;");

  is(ruleEditor.rule.textProps.length, 3, "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 4, "Should have created new property editors.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "red", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "width", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "100px", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "height", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "100px", "Should have correct property value");

  yield inspector.once("inspector-updated");
}

function* testCreateNewMultiUnfinished(inspector, view) {
  let ruleEditor = yield createAndSelectNewElement(inspector, view);
  yield createNewProperty(ruleEditor,
    "color:blue;background : orange   ; text-align:center; border-color: ");

  is(ruleEditor.rule.textProps.length, 4, "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 4, "Should have created property editors.");

  for (let ch of "red") {
    EventUtils.sendChar(ch, view.doc.defaultView);
  }
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(ruleEditor.rule.textProps.length, 4, "Should have the same number of text properties.");
  is(ruleEditor.propertyList.children.length, 5, "Should have added the changed value editor.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "blue", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "background", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "text-align", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "center", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "border-color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "red", "Should have correct property value");

  yield inspector.once("inspector-updated");
}

function* testCreateNewMultiPartialUnfinished(inspector, view) {
  let ruleEditor = yield createAndSelectNewElement(inspector, view);
  yield createNewProperty(ruleEditor, "width: 100px; heig");

  is(ruleEditor.rule.textProps.length, 2, "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 2, "Should have created a property editor.");

  // Value is focused, lets add multiple rules here and make sure they get added
  let valueEditor = ruleEditor.propertyList.children[1].querySelector("input");
  valueEditor.value = "10px;background:orangered;color: black;";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(ruleEditor.rule.textProps.length, 4, "Should have added the changed value.");
  is(ruleEditor.propertyList.children.length, 5, "Should have added the changed value editor.");

  is(ruleEditor.rule.textProps[0].name, "width", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "100px", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "heig", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "10px", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "background", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "orangered", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "black", "Should have correct property value");

  yield inspector.once("inspector-updated");
}

function* testMultiValues(inspector, view) {
  let ruleEditor = yield createAndSelectNewElement(inspector, view);
  yield createNewProperty(ruleEditor, "width:");

  is(ruleEditor.rule.textProps.length, 1, "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 1, "Should have created a property editor.");

  // Value is focused, lets add multiple rules here and make sure they get added
  let valueEditor = ruleEditor.propertyList.children[0].querySelector("input");
  valueEditor.value = "height: 10px;color:blue"
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(ruleEditor.rule.textProps.length, 2, "Should have added the changed value.");
  is(ruleEditor.propertyList.children.length, 3, "Should have added the changed value editor.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  is(ruleEditor.propertyList.children.length, 2, "Should have removed the value editor.");

  is(ruleEditor.rule.textProps[0].name, "width", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "height: 10px", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "blue", "Should have correct property value");

  yield inspector.once("inspector-updated");
}
