/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test changing display:flex to display:block and back on the flex container
// hides the flexbox highlighter.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);

  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const { highlighters, store } = inspector;

  const onFlexHighlighterToggleRendered = waitForDOM(doc, "#flexbox-checkbox-toggle");
  await selectNode("#container", inspector);
  const [flexHighlighterToggle] = await onFlexHighlighterToggleRendered;

  info("Checking the initial state of the Flexbox Inspector.");
  ok(flexHighlighterToggle, "The flexbox highlighter toggle is rendered.");
  ok(!flexHighlighterToggle.checked, "The flexbox highlighter toggle is unchecked.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");

  await toggleHighlighterON(flexHighlighterToggle, highlighters, store);

  info("Checking the flexbox highlighter is created.");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");
  ok(flexHighlighterToggle.checked, "The flexbox highlighter toggle is checked.");

  const ruleView = selectRuleView(inspector);

  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const displayProp = rule.textProps[3];
  let onBoxModelUpdated = inspector.once("boxmodel-view-updated");
  await setRuleViewProperty(ruleView, displayProp, "block");
  await onBoxModelUpdated;

  info("Checking the flexbox highlighter is not shown after setting display:block.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");
  ok(!flexHighlighterToggle.checked, "The flexbox highlighter toggle is unchecked.");

  onBoxModelUpdated = inspector.once("boxmodel-view-updated");
  await setRuleViewProperty(ruleView, displayProp, "flex");
  await onBoxModelUpdated;

  info("Checking the flexbox highlighter is *still* not shown.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");
  ok(!flexHighlighterToggle.checked, "The flexbox highlighter toggle is unchecked.");
});
