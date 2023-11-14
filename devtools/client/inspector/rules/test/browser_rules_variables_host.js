/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test matched selectors and variables defined for a :host selector.

const SHADOW_DOM = `<style>
  :host {
    --test-color: red;
  }

  span {
    color: var(--test-color);
  }
</style>
<span class="test-span">test</span>`;

const TEST_PAGE = `
  <div id="host"></div>
  <script>
    const div = document.querySelector("div");
    div.attachShadow({ mode: "open" }).innerHTML = \`${SHADOW_DOM}\`;
  </script>`;

const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(
  TEST_PAGE
)}`;

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  info("Select the host and check that :host is matching");
  await selectNode("#host", inspector);
  let selector = getRuleViewRuleEditor(view, 1).selectorText;
  is(
    selector.querySelector(".matched").textContent,
    ":host",
    ":host should be matched."
  );

  info("Select a shadow dom element and check that :host is matching");
  const nodeFront = await getNodeFrontInShadowDom(
    ".test-span",
    "#host",
    inspector
  );
  await selectNode(nodeFront, inspector);

  selector = getRuleViewRuleEditor(view, 3).selectorText;
  is(
    selector.querySelector(".matched").textContent,
    ":host",
    ":host should be matched."
  );

  info("Check that the variable from :host is correctly applied");
  const setColor = getRuleViewProperty(
    view,
    "span",
    "color"
  ).valueSpan.querySelector(".ruleview-variable");
  is(setColor.textContent, "--test-color", "--test-color is set correctly");
  is(
    setColor.dataset.variable,
    "--test-color = red",
    "--test-color's dataset.variable is set correctly"
  );
  const previewTooltip = await assertShowPreviewTooltip(view, setColor);
  ok(
    previewTooltip.panel.textContent.includes("--test-color = red"),
    "CSS variable preview tooltip shows the expected CSS variable"
  );
  await assertTooltipHiddenOnMouseOut(previewTooltip, setColor);
});
