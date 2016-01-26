/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that locking the pseudoclass displays correctly in the ruleview

const PSEUDO = ":hover";
const TEST_URL = 'data:text/html;charset=UTF-8,' +
                 '<head>' +
                 '  <style>div {color:red;} div:hover {color:blue;}</style>' +
                 '</head>' +
                 '<body>' +
                 '  <div id="parent-div">' +
                 '    <div id="div-1">test div</div>' +
                 '    <div id="div-2">test div2</div>' +
                 '  </div>' +
                 '</body>';

add_task(function*() {
  info("Creating the test tab and opening the rule-view");
  let {toolbox, inspector, testActor} = yield openInspectorForURL(TEST_URL);

  info("Selecting the ruleview sidebar");
  inspector.sidebar.select("ruleview");

  let view = inspector.ruleview.view;

  info("Selecting the test node");
  yield selectNode("#div-1", inspector);

  yield togglePseudoClass(inspector);
  yield assertPseudoAddedToNode(inspector, testActor, view);

  yield togglePseudoClass(inspector);
  yield assertPseudoRemovedFromNode(testActor);
  yield assertPseudoRemovedFromView(inspector, testActor, view);

  yield togglePseudoClass(inspector);
  yield testNavigate(inspector, testActor, view);

  info("Destroying the toolbox");
  let tab = toolbox.target.tab;
  yield toolbox.destroy();

  // As the toolbox get detroyed, we need to fetch a new test-actor
  testActor = yield getTestActorWithoutToolbox(tab);

  yield assertPseudoRemovedFromNode(testActor);
});


function* togglePseudoClass(inspector) {
  info("Toggle the pseudoclass, wait for it to be applied");

  // Give the inspector panels a chance to update when the pseudoclass changes
  let onPseudo = inspector.selection.once("pseudoclass");
  let onRefresh = inspector.once("rule-view-refreshed");

  // Walker uses SDK-events so calling walker.once does not return a promise.
  let onMutations = once(inspector.walker, "mutations");

  yield inspector.togglePseudoClass(PSEUDO);

  yield onPseudo;
  yield onRefresh;
  yield onMutations;
}

function* testNavigate(inspector, testActor, ruleview) {
  yield selectNode("#parent-div", inspector);

  info("Make sure the pseudoclass is still on after navigating to a parent");

  ok((yield testActor.hasPseudoClassLock("#div-1", PSEUDO)), "pseudo-class lock is still applied after inspecting ancestor");

  let onPseudo = inspector.selection.once("pseudoclass");
  yield selectNode("#div-2", inspector);
  yield onPseudo;

  info("Make sure the pseudoclass is removed after navigating to a non-hierarchy node");
  ok(!(yield testActor.hasPseudoClassLock("#div-1", PSEUDO)), "pseudo-class lock is removed after inspecting sibling node");

  yield selectNode("#div-1", inspector);
  yield togglePseudoClass(inspector);
}

function* showPickerOn(selector, inspector) {
  let highlighter = inspector.toolbox.highlighter;
  let nodeFront = yield getNodeFront(selector, inspector);
  yield highlighter.showBoxModel(nodeFront);
}

function* assertPseudoAddedToNode(inspector, testActor, ruleview) {
  info("Make sure the pseudoclass lock is applied to #div-1 and its ancestors");

  let hasLock = yield testActor.hasPseudoClassLock("#div-1", PSEUDO);
  ok(hasLock, "pseudo-class lock has been applied");
  hasLock = yield testActor.hasPseudoClassLock("#parent-div", PSEUDO);
  ok(hasLock, "pseudo-class lock has been applied");
  hasLock = yield testActor.hasPseudoClassLock("body", PSEUDO);
  ok(hasLock, "pseudo-class lock has been applied");

  info("Check that the ruleview contains the pseudo-class rule");
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 3, "rule view is showing 3 rules for pseudo-class locked div");
  is(rules[1]._ruleEditor.rule.selectorText, "div:hover", "rule view is showing " + PSEUDO + " rule");

  info("Show the highlighter on #div-1");
  yield showPickerOn("#div-1", inspector);

  info("Check that the infobar selector contains the pseudo-class");
  let value = yield testActor.getHighlighterNodeTextContent("box-model-nodeinfobar-pseudo-classes");
  is(value, PSEUDO, "pseudo-class in infobar selector");
  yield inspector.toolbox.highlighter.hideBoxModel();
}

function* assertPseudoRemovedFromNode(testActor) {
  info("Make sure the pseudoclass lock is removed from #div-1 and its ancestors");

  let hasLock = yield testActor.hasPseudoClassLock("#div-1", PSEUDO);
  ok(!hasLock, "pseudo-class lock has been removed");
  hasLock = yield testActor.hasPseudoClassLock("#parent-div", PSEUDO);
  ok(!hasLock, "pseudo-class lock has been removed");
  hasLock = yield testActor.hasPseudoClassLock("body", PSEUDO);
  ok(!hasLock, "pseudo-class lock has been removed");
}

function* assertPseudoRemovedFromView(inspector, testActor, ruleview) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 2, "rule view is showing 2 rules after removing lock");

  yield showPickerOn("#div-1", inspector);

  let value = yield testActor.getHighlighterNodeTextContent("box-model-nodeinfobar-pseudo-classes");
  is(value, "", "pseudo-class removed from infobar selector");
  yield inspector.toolbox.highlighter.hideBoxModel();
}
