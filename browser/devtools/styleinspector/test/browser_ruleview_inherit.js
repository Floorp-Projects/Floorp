/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that inherited properties appear as such in the rule-view

let {ELEMENT_STYLE} = devtools.require("devtools/server/actors/styles");

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_inspector_changes.js");
  let {toolbox, inspector, view} = yield openRuleView();

  yield simpleInherit(inspector, view);
  yield emptyInherit(inspector, view);
  yield elementStyleInherit(inspector, view);
});

function* simpleInherit(inspector, view) {
  let style = '' +
    '#test2 {' +
    '  background-color: green;' +
    '  color: purple;' +
    '}';

  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = '<div id="test2"><div id="test1">Styled Node</div></div>';

  yield selectNode("#test1", inspector);

  let elementStyle = view._elementStyle;
  is(elementStyle.rules.length, 2, "Should have 2 rules.");

  let elementRule = elementStyle.rules[0];
  ok(!elementRule.inherited, "Element style attribute should not consider itself inherited.");

  let inheritRule = elementStyle.rules[1];
  is(inheritRule.selectorText, "#test2", "Inherited rule should be the one that includes inheritable properties.");
  ok(!!inheritRule.inherited, "Rule should consider itself inherited.");
  is(inheritRule.textProps.length, 1, "Should only display one inherited style");
  let inheritProp = inheritRule.textProps[0];
  is(inheritProp.name, "color", "color should have been inherited.");

  styleNode.remove();
}

function* emptyInherit(inspector, view) {
  // No inheritable styles, this rule shouldn't show up.
  let style = '' +
    '#test2 {' +
    '  background-color: green;' +
    '}';

  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = '<div id="test2"><div id="test1">Styled Node</div></div>';

  yield selectNode("#test1", inspector);

  let elementStyle = view._elementStyle;
  is(elementStyle.rules.length, 1, "Should have 1 rule.");

  let elementRule = elementStyle.rules[0];
  ok(!elementRule.inherited, "Element style attribute should not consider itself inherited.");

  styleNode.parentNode.removeChild(styleNode);
}

function* elementStyleInherit(inspector, view) {
  content.document.body.innerHTML = '<div id="test2" style="color: red"><div id="test1">Styled Node</div></div>';

  yield selectNode("#test1", inspector);

  let elementStyle = view._elementStyle;
  is(elementStyle.rules.length, 2, "Should have 2 rules.");

  let elementRule = elementStyle.rules[0];
  ok(!elementRule.inherited, "Element style attribute should not consider itself inherited.");

  let inheritRule = elementStyle.rules[1];
  is(inheritRule.domRule.type, ELEMENT_STYLE, "Inherited rule should be an element style, not a rule.");
  ok(!!inheritRule.inherited, "Rule should consider itself inherited.");
  is(inheritRule.textProps.length, 1, "Should only display one inherited style");
  let inheritProp = inheritRule.textProps[0];
  is(inheritProp.name, "color", "color should have been inherited.");
}
