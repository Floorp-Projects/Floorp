/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the display of overridden declarations in the rule-view

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_override.js");
  let {toolbox, inspector, view} = yield openRuleView();

  yield simpleOverride(inspector, view);
  yield partialOverride(inspector, view);
  yield importantOverride(inspector, view);
  yield disableOverride(inspector, view);
});

function* createTestContent(inspector, style) {
  let onMutated = inspector.once("markupmutation");
  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = '<div id="testid" class="testclass">Styled Node</div>';
  yield onMutated;
  yield selectNode("#testid", inspector);
  return styleNode;
}

function* removeTestContent(inspector, node) {
  let onMutated = inspector.once("markupmutation");
  node.remove();
  yield onMutated;
}

function* simpleOverride(inspector, view) {
  let styleNode = yield createTestContent(inspector, '' +
    '#testid {' +
    '  background-color: blue;' +
    '} ' +
    '.testclass {' +
    '  background-color: green;' +
    '}');

  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];
  is(idProp.name, "background-color", "First ID prop should be background-color");
  ok(!idProp.overridden, "ID prop should not be overridden.");

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  is(classProp.name, "background-color", "First class prop should be background-color");
  ok(classProp.overridden, "Class property should be overridden.");

  // Override background-color by changing the element style.
  let elementRule = elementStyle.rules[0];
  elementRule.createProperty("background-color", "purple", "");
  yield elementRule._applyingModifications;

  let elementProp = elementRule.textProps[0];
  is(classProp.name, "background-color", "First element prop should now be background-color");
  ok(!elementProp.overridden, "Element style property should not be overridden");
  ok(idProp.overridden, "ID property should be overridden");
  ok(classProp.overridden, "Class property should be overridden");

  yield removeTestContent(inspector, styleNode);
}

function* partialOverride(inspector, view) {
  let styleNode = yield createTestContent(inspector, '' +
    // Margin shorthand property...
    '.testclass {' +
    '  margin: 2px;' +
    '}' +
    // ... will be partially overridden.
    '#testid {' +
    '  margin-left: 1px;' +
    '}');

  let elementStyle = view._elementStyle;

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  ok(!classProp.overridden,
    "Class prop shouldn't be overridden, some props are still being used.");

  for (let computed of classProp.computed) {
    if (computed.name.indexOf("margin-left") == 0) {
      ok(computed.overridden, "margin-left props should be overridden.");
    } else {
      ok(!computed.overridden, "Non-margin-left props should not be overridden.");
    }
  }

  yield removeTestContent(inspector, styleNode);
}

function* importantOverride(inspector, view) {
  let styleNode = yield createTestContent(inspector, '' +
    // Margin shorthand property...
    '.testclass {' +
    '  background-color: green !important;' +
    '}' +
    // ... will be partially overridden.
    '#testid {' +
    '  background-color: blue;' +
    '}');

  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];
  ok(idProp.overridden, "Not-important rule should be overridden.");

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  ok(!classProp.overridden, "Important rule should not be overridden.");

  yield removeTestContent(inspector, styleNode);

  let elementRule = elementStyle.rules[0];
  let elementProp = elementRule.createProperty("background-color", "purple", "important");
  yield elementRule._applyingModifications;

  ok(classProp.overridden, "New important prop should override class property.");
  ok(!elementProp.overridden, "New important prop should not be overriden.");
}

function* disableOverride(inspector, view) {
  let styleNode = yield createTestContent(inspector, '' +
    '#testid {' +
    '  background-color: blue;' +
    '}' +
    '.testclass {' +
    '  background-color: green;' +
    '}');

  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];

  idProp.setEnabled(false);
  yield idRule._applyingModifications;

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  ok(!classProp.overridden, "Class prop should not be overridden after id prop was disabled.");

  yield removeTestContent(inspector, styleNode);
}
