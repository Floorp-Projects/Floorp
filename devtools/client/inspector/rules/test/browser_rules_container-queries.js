/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct when the page defines container queries.
const TEST_URI = `
  <!DOCTYPE html>
  <style type="text/css">
    body {
      container: mycontainer / inline-size;
    }

    section {
      container: mycontainer / inline-size;
    }

    @container (width > 0px) {
      h1, [test-hint="nocontainername"]{
        outline-color: chartreuse;
      }
    }

    @container unknowncontainer (min-width: 2vw) {
      h1, [test-hint="unknowncontainer"] {
        border-color: salmon;
      }
    }

    @container mycontainer (1px < width < 10000px) {
      h1, [test-hint="container"] {
        color: tomato;
      }

      section, [test-hint="container-duplicate-name--body"] {
        color: gold;
      }

      div, [test-hint="container-duplicate-name--section"] {
        color: salmon;
      }
    }
  </style>
  <h1>Hello @container!</h1>
  <section>
    <div>
      <h2>You rock</h2>
    </div>
  </section>
`;

add_task(async function() {
  await pushPref("layout.css.container-queries.enabled", true);

  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);
  assertContainerQueryData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `h1, [test-hint="container"]`,
      ancestorRulesData: ["@container mycontainer (1px < width < 10000px)"],
    },
    {
      selector: `h1, [test-hint="unknowncontainer"]`,
      ancestorRulesData: ["@container unknowncontainer (min-width: 2vw)"],
    },
    {
      selector: `h1, [test-hint="nocontainername"]`,
      ancestorRulesData: ["@container (width > 0px)"],
    },
  ]);

  info("Check that the 'jump to container' button works as expected");
  await assertJumpToContainerButton(inspector, view, 1, "body");

  info(
    "Check that we do fallback to the documentElement node when the container name isn't set anywhere"
  );
  await selectNode("h1", inspector);
  await assertJumpToContainerButton(inspector, view, 2, "html");

  info("Check that inherited rules display container query data as expected");
  await selectNode("h2", inspector);

  assertContainerQueryData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `div, [test-hint="container-duplicate-name--section"]`,
      ancestorRulesData: ["@container mycontainer (1px < width < 10000px)"],
    },
    {
      selector: `section, [test-hint="container-duplicate-name--body"]`,
      ancestorRulesData: ["@container mycontainer (1px < width < 10000px)"],
    },
  ]);

  info(
    "Check that the 'jump to container' button works as expected for inherited rules"
  );
  await assertJumpToContainerButton(inspector, view, 1, "section");

  await selectNode("h2", inspector);
  await assertJumpToContainerButton(inspector, view, 2, "body");
});

function assertContainerQueryData(view, expectedRules) {
  const rulesInView = Array.from(
    view.element.querySelectorAll(".ruleview-rule")
  );

  is(
    rulesInView.length,
    expectedRules.length,
    "All expected rules are displayed"
  );

  for (let i = 0; i < expectedRules.length; i++) {
    const expectedRule = expectedRules[i];
    info(`Checking rule #${i}: ${expectedRule.selector}`);

    const selector = rulesInView[i].querySelector(".ruleview-selectorcontainer")
      .innerText;
    is(selector, expectedRule.selector, `Expected selector for ${selector}`);

    const ancestorDataEl = getRuleViewAncestorRulesDataElementByIndex(view, i);

    if (expectedRule.ancestorRulesData == null) {
      is(
        ancestorDataEl,
        null,
        `No ancestor rules data displayed for ${selector}`
      );
    } else {
      is(
        ancestorDataEl?.innerText,
        expectedRule.ancestorRulesData.join("\n"),
        `Expected ancestor rules data displayed for ${selector}`
      );
      ok(
        ancestorDataEl.querySelector(".container-query .open-inspector") !==
          null,
        "An icon is displayed to select the container in the markup view"
      );
    }
  }
}

async function assertJumpToContainerButton(
  inspector,
  view,
  ruleIndex,
  expectedSelectedNodeAfterClick
) {
  const selectContainerButton = getRuleViewAncestorRulesDataElementByIndex(
    view,
    ruleIndex
  ).querySelector(".open-inspector");

  // Ensure that the button can be targetted from EventUtils.
  selectContainerButton.scrollIntoView();

  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  const onNodeHighlight = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  EventUtils.synthesizeMouseAtCenter(
    selectContainerButton,
    { type: "mouseover" },
    selectContainerButton.ownerDocument.defaultView
  );
  const { nodeFront: highlightedNodeFront } = await onNodeHighlight;
  is(
    highlightedNodeFront.displayName,
    expectedSelectedNodeAfterClick,
    "The correct node was highlighted"
  );

  const onceNewNodeFront = inspector.selection.once("new-node-front");
  const onNodeUnhighlight = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );

  EventUtils.synthesizeMouseAtCenter(
    selectContainerButton,
    {},
    selectContainerButton.ownerDocument.defaultView
  );

  const nodeFront = await onceNewNodeFront;
  is(
    nodeFront.displayName,
    expectedSelectedNodeAfterClick,
    "The correct node has been selected"
  );

  await onNodeUnhighlight;
  ok("Highlighter was hidden when clicking on icon");
}
