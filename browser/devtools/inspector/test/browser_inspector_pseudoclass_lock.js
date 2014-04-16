/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

waitForExplicitFinish();

function test() {
  ignoreAllUncaughtExceptions();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    waitForFocus(startTests, content);
  }, true);

  content.location = TEST_URL;
}

let startTests = Task.async(function*() {
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#div-1", inspector);

  yield performTests(inspector, view);

  yield finishUp(toolbox);
  finish();
});

function* performTests(inspector, ruleview) {
  yield togglePseudoClass(inspector);
  yield testAdded(inspector, ruleview);

  yield togglePseudoClass(inspector);
  yield testRemoved();
  yield testRemovedFromUI(inspector, ruleview);

  yield togglePseudoClass(inspector);
  yield testNavigate(inspector, ruleview);
}

function* togglePseudoClass(inspector) {
  info("Toggle the pseudoclass, wait for the pseudoclass event and wait for the refresh of the rule view");

  let onPseudo = inspector.selection.once("pseudoclass");
  let onRefresh = inspector.once("rule-view-refreshed");
  inspector.togglePseudoClass(PSEUDO);

  yield onPseudo;
  yield onRefresh;
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

function* testAdded(inspector, ruleview) {
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

function* testRemoved() {
  info("Make sure the pseudoclass lock is removed from #div-1 and its ancestors");
  let node = getNode("#div-1");
  do {
    is(DOMUtils.hasPseudoClassLock(node, PSEUDO), false,
       "pseudo-class lock has been removed");
    node = node.parentNode;
  } while (node.parentNode)
}

function* testRemovedFromUI(inspector, ruleview) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  let rules = ruleview.element.querySelectorAll(".ruleview-rule.theme-separator");
  is(rules.length, 2, "rule view is showing 2 rules after removing lock");

  yield showPickerOn(getNode("#div-1"), inspector);

  let pseudoClassesBox = getHighlighter().querySelector(".highlighter-nodeinfobar-pseudo-classes");
  is(pseudoClassesBox.textContent, "", "pseudo-class removed from infobar selector");
  yield inspector.toolbox.highlighter.hideBoxModel();
}

function* finishUp(toolbox) {
  let onDestroy = gDevTools.once("toolbox-destroyed");
  toolbox.destroy();
  yield onDestroy;

  yield testRemoved(getNode("#div-1"));
  gBrowser.removeCurrentTab();
}
