/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the flexbox inspector highlighter state when the layout panel is selected after
// toggling on the flexbox highlighter from the rules view.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await pushPref("devtools.inspector.activeSidebar", "computedview");
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  const { styleDocument: doc } = view;
  const { highlighters } = inspector;

  await selectNode("#container", inspector);

  const container = getRuleViewProperty(view, ".container", "display").valueSpan;
  const flexboxToggle = container.querySelector(".ruleview-flex");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  await onHighlighterShown;

  info("Selecting the layout panel");
  const onFlexHighlighterToggleRendered = waitForDOM(doc, "#flexbox-checkbox-toggle");
  inspector.sidebar.select("layoutview");
  const [flexHighlighterToggle] = await onFlexHighlighterToggleRendered;

  info("Checking the state of the Flexbox Inspector.");
  ok(flexHighlighterToggle, "The flexbox highlighter toggle is rendered.");
  ok(flexHighlighterToggle.checked, "The flexbox highlighter toggle is checked.");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");
});
