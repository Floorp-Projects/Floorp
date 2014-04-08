/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;

let inspector;
let view;

let {ELEMENT_STYLE} = devtools.require("devtools/server/actors/styles");

function simpleInherit(aInspector, aRuleView)
{
  inspector = aInspector;
  view = aRuleView;

  let style = '' +
    '#test2 {' +
    '  background-color: green;' +
    '  color: purple;' +
    '}';

  let styleNode = addStyle(doc, style);
  doc.body.innerHTML = '<div id="test2"><div id="test1">Styled Node</div></div>';

  inspector.selection.setNode(doc.getElementById("test1"));
  inspector.once("inspector-updated", () => {
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

    styleNode.parentNode.removeChild(styleNode);

    emptyInherit();
  });
}

function emptyInherit()
{
  // No inheritable styles, this rule shouldn't show up.
  let style = '' +
    '#test2 {' +
    '  background-color: green;' +
    '}';

  let styleNode = addStyle(doc, style);
  doc.body.innerHTML = '<div id="test2"><div id="test1">Styled Node</div></div>';

  inspector.selection.setNode(doc.getElementById("test1"));
  inspector.once("inspector-updated", () => {
    let elementStyle = view._elementStyle;

    is(elementStyle.rules.length, 1, "Should have 1 rule.");

    let elementRule = elementStyle.rules[0];
    ok(!elementRule.inherited, "Element style attribute should not consider itself inherited.");

    styleNode.parentNode.removeChild(styleNode);

    elementStyleInherit();
  });
}

function elementStyleInherit()
{
  doc.body.innerHTML = '<div id="test2" style="color: red"><div id="test1">Styled Node</div></div>';

  inspector.selection.setNode(doc.getElementById("test1"));
  inspector.once("inspector-updated", () => {
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

    finishTest();
  });
}

function finishTest()
{
  doc = inspector = view = null;
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
    waitForFocus(() => openRuleView(simpleInherit), content);
  }, true);

  content.location = "data:text/html;charset=utf-8,browser_inspector_changes.js";
}
