/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals getHighlighterTestFrontWithoutToolbox */
"use strict";

// Test that locking the pseudoclass displays correctly in the ruleview

const PSEUDO = ":hover";
const TEST_URL =
  "data:text/html;charset=UTF-8," +
  "<head>" +
  "  <style>div {color:red;} div:hover {color:blue;}</style>" +
  "</head>" +
  "<body>" +
  '  <div id="parent-div">' +
  '    <div id="div-1">test div</div>' +
  '    <div id="div-2">test div2</div>' +
  "  </div>" +
  "</body>";

add_task(async function () {
  info("Creating the test tab and opening the rule-view");
  let { tab, toolbox, inspector, highlighterTestFront } =
    await openInspectorForURL(TEST_URL);

  info("Selecting the ruleview sidebar");
  inspector.sidebar.select("ruleview");

  const view = inspector.getPanel("ruleview").view;

  info("Selecting the test node");
  await selectNode("#div-1", inspector);

  await togglePseudoClass(inspector);
  await assertPseudoAddedToNode(
    inspector,
    highlighterTestFront,
    view,
    "#div-1"
  );

  await togglePseudoClass(inspector);
  await assertPseudoRemovedFromNode(highlighterTestFront, "#div-1");
  await assertPseudoRemovedFromView(
    inspector,
    highlighterTestFront,
    view,
    "#div-1"
  );

  await togglePseudoClass(inspector);
  await testNavigate(inspector);

  info("Toggle pseudo on the parent and ensure everything is toggled off");
  await selectNode("#parent-div", inspector);
  await togglePseudoClass(inspector);
  await assertPseudoRemovedFromNode(highlighterTestFront, "#div-1");
  await assertPseudoRemovedFromView(
    inspector,
    highlighterTestFront,
    view,
    "#div-1"
  );

  await togglePseudoClass(inspector);
  info("Assert pseudo is dismissed when toggling it on a sibling node");
  await selectNode("#div-2", inspector);
  await togglePseudoClass(inspector);
  await assertPseudoAddedToNode(
    inspector,
    highlighterTestFront,
    view,
    "#div-2"
  );
  const hasLock = await hasPseudoClassLock("#div-1", PSEUDO);
  ok(
    !hasLock,
    "pseudo-class lock has been removed for the previous locked node"
  );

  info("Destroying the toolbox");
  await toolbox.destroy();

  // As the toolbox get destroyed, we need to fetch a new test-actor
  highlighterTestFront = await getHighlighterTestFrontWithoutToolbox(tab);

  await assertPseudoRemovedFromNode(highlighterTestFront, "#div-1");
  await assertPseudoRemovedFromNode(highlighterTestFront, "#div-2");
});

async function togglePseudoClass(inspector) {
  info("Toggle the pseudoclass, wait for it to be applied");

  // Give the inspector panels a chance to update when the pseudoclass changes
  const onPseudo = inspector.selection.once("pseudoclass");
  const onRefresh = inspector.once("rule-view-refreshed");

  // Walker uses SDK-events so calling walker.once does not return a promise.
  const onMutations = once(inspector.walker, "mutations");

  await inspector.togglePseudoClass(PSEUDO);

  await onPseudo;
  await onRefresh;
  await onMutations;
}

async function testNavigate(inspector) {
  await selectNode("#parent-div", inspector);

  info("Make sure the pseudoclass is still on after navigating to a parent");

  ok(
    await hasPseudoClassLock("#div-1", PSEUDO),
    "pseudo-class lock is still applied after inspecting ancestor"
  );

  await selectNode("#div-2", inspector);

  info(
    "Make sure the pseudoclass is still set after navigating to a " +
      "non-hierarchy node"
  );
  ok(
    await hasPseudoClassLock("#div-1", PSEUDO),
    "pseudo-class lock is still on after inspecting sibling node"
  );

  await selectNode("#div-1", inspector);
}

async function assertPseudoAddedToNode(
  inspector,
  highlighterTestFront,
  ruleview,
  selector
) {
  info(
    "Make sure the pseudoclass lock is applied to " +
      selector +
      " and its ancestors"
  );

  let hasLock = await hasPseudoClassLock(selector, PSEUDO);
  ok(hasLock, "pseudo-class lock has been applied");
  hasLock = await hasPseudoClassLock("#parent-div", PSEUDO);
  ok(hasLock, "pseudo-class lock has been applied");
  hasLock = await hasPseudoClassLock("body", PSEUDO);
  ok(hasLock, "pseudo-class lock has been applied");

  info("Check that the ruleview contains the pseudo-class rule");
  const rules = ruleview.element.querySelectorAll(".ruleview-rule");
  is(
    rules.length,
    3,
    "rule view is showing 3 rules for pseudo-class locked div"
  );
  is(
    rules[1]._ruleEditor.rule.selectorText,
    "div:hover",
    "rule view is showing " + PSEUDO + " rule"
  );

  info("Show the highlighter on " + selector);
  const nodeFront = await getNodeFront(selector, inspector);
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    nodeFront
  );

  info("Check that the infobar selector contains the pseudo-class");
  const value = await highlighterTestFront.getHighlighterNodeTextContent(
    "box-model-infobar-pseudo-classes"
  );
  is(value, PSEUDO, "pseudo-class in infobar selector");
  await inspector.highlighters.hideHighlighterType(
    inspector.highlighters.TYPES.BOXMODEL
  );
}

async function assertPseudoRemovedFromNode(highlighterTestFront, selector) {
  info(
    "Make sure the pseudoclass lock is removed from #div-1 and its " +
      "ancestors"
  );

  let hasLock = await hasPseudoClassLock(selector, PSEUDO);
  ok(!hasLock, "pseudo-class lock has been removed");
  hasLock = await hasPseudoClassLock("#parent-div", PSEUDO);
  ok(!hasLock, "pseudo-class lock has been removed");
  hasLock = await hasPseudoClassLock("body", PSEUDO);
  ok(!hasLock, "pseudo-class lock has been removed");
}

async function assertPseudoRemovedFromView(
  inspector,
  highlighterTestFront,
  ruleview,
  selector
) {
  info("Check that the ruleview no longer contains the pseudo-class rule");
  const rules = ruleview.element.querySelectorAll(".ruleview-rule");
  is(rules.length, 2, "rule view is showing 2 rules after removing lock");

  const nodeFront = await getNodeFront(selector, inspector);
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    nodeFront
  );

  const value = await highlighterTestFront.getHighlighterNodeTextContent(
    "box-model-infobar-pseudo-classes"
  );
  is(value, "", "pseudo-class removed from infobar selector");
  await inspector.highlighters.hideHighlighterType(
    inspector.highlighters.TYPES.BOXMODEL
  );
}

/**
 * Check that an element currently has a pseudo-class lock.
 * @param {String} selector The node selector to get the pseudo-class from
 * @param {String} pseudo The pseudoclass to check for
 * @return {Promise<Boolean>}
 */
function hasPseudoClassLock(selector, pseudoClass) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, pseudoClass],
    (_selector, _pseudoClass) => {
      const element = content.document.querySelector(_selector);
      return InspectorUtils.hasPseudoClassLock(element, _pseudoClass);
    }
  );
}
