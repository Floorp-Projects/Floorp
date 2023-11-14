/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that selector warnings are displayed in the rule-view.
const TEST_URI = `
  <!DOCTYPE html>
  <style>
    main, :has(form) {
      /* /!\ space between & and : is important */
      & :has(input),
      & :has(select),
      &:has(button) {
        background: gold;
      }
    }
  </style>
  <body>
    <main>
      <form>
        <input>
      </form>
    </main>
  </body>`;

const UNCONSTRAINED_HAS_WARNING_MESSAGE =
  "This selector uses unconstrained :has(), which can be slow";

add_task(async function () {
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("main", inspector);
  const { ancestorDataEl, selectorText } = getRuleViewRuleEditor(view, 1);

  info(
    "Check that unconstrained :has() warnings are displayed for the rules selectors"
  );
  const ruleSelectors = Array.from(
    selectorText.querySelectorAll(".ruleview-selector")
  );

  await assertSelectorWarnings({
    view,
    selectorEl: ruleSelectors[0],
    selectorText: "& :has(input)",
    expectedWarnings: [UNCONSTRAINED_HAS_WARNING_MESSAGE],
  });
  await assertSelectorWarnings({
    view,
    selectorEl: ruleSelectors[1],
    selectorText: "& :has(select)",
    expectedWarnings: [UNCONSTRAINED_HAS_WARNING_MESSAGE],
  });
  // Warning is not displayed when the selector does not have warnings
  await assertSelectorWarnings({
    view,
    selectorEl: ruleSelectors[2],
    selectorText: "&:has(button)",
    expectedWarnings: [],
  });

  info(
    "Check that unconstrained :has() warnings are displayed for the parent rules selectors"
  );
  const parentRuleSelectors = Array.from(
    ancestorDataEl.querySelectorAll(".ruleview-selector")
  );
  await assertSelectorWarnings({
    view,
    selectorEl: parentRuleSelectors[0],
    selectorText: "main",
    expectedWarnings: [],
  });
  await assertSelectorWarnings({
    view,
    selectorEl: parentRuleSelectors[1],
    selectorText: ":has(form)",
    expectedWarnings: [UNCONSTRAINED_HAS_WARNING_MESSAGE],
  });
});

async function assertSelectorWarnings({
  view,
  selectorEl,
  selectorText,
  expectedWarnings,
}) {
  is(
    selectorEl.textContent,
    selectorText,
    "Passed selector element is the expected one"
  );

  const selectorWarningsContainerEl = selectorEl.querySelector(
    ".ruleview-selector-warnings"
  );

  if (expectedWarnings.length === 0) {
    ok(
      selectorWarningsContainerEl === null,
      `"${selectorText}" does not have warnings`
    );
    return;
  }

  ok(
    selectorWarningsContainerEl !== null,
    `"${selectorText}" does have warnings`
  );

  is(
    selectorWarningsContainerEl
      .getAttribute("data-selector-warning-kind")
      ?.split(",")?.length || 0,
    expectedWarnings.length,
    `"${selectorText}" has expected number of warnings`
  );

  // Ensure that the element can be targetted from EventUtils.
  selectorWarningsContainerEl.scrollIntoView();

  const tooltip = view.tooltips.getTooltip("interactiveTooltip");
  const onTooltipReady = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(
    selectorWarningsContainerEl,
    { type: "mousemove" },
    selectorWarningsContainerEl.ownerDocument.defaultView
  );
  await onTooltipReady;

  const lis = Array.from(tooltip.panel.querySelectorAll("li")).map(
    li => li.textContent
  );
  Assert.deepEqual(lis, expectedWarnings, "Tooltip has expected items");

  info("Hide the tooltip");
  const onHidden = tooltip.once("hidden");
  // Move the mouse elsewhere to hide the tooltip
  EventUtils.synthesizeMouse(
    selectorWarningsContainerEl.ownerDocument.body,
    1,
    1,
    { type: "mousemove" },
    selectorWarningsContainerEl.ownerDocument.defaultView
  );
  await onHidden;
}
