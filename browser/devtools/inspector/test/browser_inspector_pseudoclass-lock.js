/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that locking the pseudoclass displays correctly in the ruleview

let DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
const PSEUDO = ":hover";
const TEST_URL = 'data:text/html,' +
                 '<head>' +
                 '  <style>div {color:red;} div:hover {color:blue;}</style>' +
                 '</head>' +
                 '<body>' +
                 '  <div id="parent-div">' +
                 '    <div id="div-1">test div</div>' +
                 '    <div id="div-2">test div2</div>' +
                 '  </div>' +
                 '</body>';

let test = asyncTest(function*() {
  info("Creating the test tab and opening the rule-view");
  let {toolbox, inspector} = yield openInspectorForURL(TEST_URL);
  let view = yield ensureRuleView(inspector);

  info("Selecting the test node");
  yield selectNode("#div-1", inspector);

  yield togglePseudoClass(inspector);
  yield assertPseudoAddedToNode(inspector, view);

  yield togglePseudoClass(inspector);
  yield assertPseudoRemovedFromNode();
  yield assertPseudoRemovedFromView(inspector, view);

  yield togglePseudoClass(inspector);
  yield testNavigate(inspector, view);

  info("Destroying the toolbox");
  yield toolbox.destroy();
  yield assertPseudoRemovedFromNode(getNode("#div-1"));
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

function* testNavigate(inspector, ruleview) {
  yield selectNode("#parent-div", inspector);

  info("Make sure the pseudoclass is still on after navigating to a parent");
  is(DOMUtils.hasPseudoClassLock(getNode("#div-1"), PSEUDO), true,
    "pseudo-class lock is still applied after inspecting ancestor");

  let onPseudo = inspector.selection.once("pseudoclass");
  yield selectNode("#div-2", inspector);
  yield onPseudo;

  info("Make sure the pseudoclass is removed after navigating to a non-hierarchy node");
  is(DOMUtils.hasPseudoClassLock(getNode("#div-1"), PSEUDO), false,
    "pseudo-class lock is removed after inspecting sibling node");

  yield selectNode("#div-1", inspector);
  yield togglePseudoClass(inspector);
  yield inspector.once("computed-view-refreshed");
}

function showPickerOn(node, inspector) {
  let highlighter = inspector.toolbox.highlighter;
  return highlighter.showBoxModel(getNodeFront(node));
}

function* assertPseudoAddedToNode(inspector, ruleview) {
  info("Make sure the pseudoclass lock is applied to #div-1 and its ancestors");
  let node = getNode("#div-1");
  do {
    is(DOMUtils.hasPseudoClassLock(node, PSEUDO), true,
      "pseudo-class lock has been applied");
    node = node.parentNode;
  } while (node.parentNode)

  info("Check that the ruleview contains the pseudo-class rule");
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 3, "rule view is showing 3 rules for pseudo-class locked div");
  is(rules[1]._ruleEditor.rule.selectorText, "div:hover", "rule view is showing " + PSEUDO + " rule");

  info("Show the highlighter on #div-1");
  yield showPickerOn(getNode("#div-1"), inspector);

  info("Check that the infobar selector contains the pseudo-class");
  let pseudoClassesBox = getHighlighter().querySelector(".highlighter-nodeinfobar-pseudo-classes");
  is(pseudoClassesBox.textContent, PSEUDO, "pseudo-class in infobar selector");
  yield inspector.toolbox.highlighter.hideBoxModel();
}

function* assertPseudoRemovedFromNode() {
  info("Make sure the pseudoclass lock is removed from #div-1 and its ancestors");
  let node = getNode("#div-1");
  do {
    is(DOMUtils.hasPseudoClassLock(node, PSEUDO), false,
       "pseudo-class lock has been removed");
    node = node.parentNode;
  } while (node.parentNode)
}

function* assertPseudoRemovedFromView(inspector, ruleview) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 2, "rule view is showing 2 rules after removing lock");

  yield showPickerOn(getNode("#div-1"), inspector);

  let pseudoClassesBox = getHighlighter().querySelector(".highlighter-nodeinfobar-pseudo-classes");
  is(pseudoClassesBox.textContent, "", "pseudo-class removed from infobar selector");
  yield inspector.toolbox.highlighter.hideBoxModel();
}

function* ensureRuleView(inspector) {
  if (!inspector.sidebar.getWindowForTab("ruleview")) {
    info("Waiting for ruleview initialization to complete.");
    yield inspector.sidebar.once("ruleview-ready");
  }

  info("Selecting the ruleview sidebar");
  inspector.sidebar.select("ruleview");

  return inspector.sidebar.getWindowForTab("ruleview")["ruleview"].view;
}
