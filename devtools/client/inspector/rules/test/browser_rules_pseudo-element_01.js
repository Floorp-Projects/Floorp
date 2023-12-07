/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pseudoelements are displayed correctly in the rule view

const TEST_URI = URL_ROOT + "doc_pseudoelement.html";
const PSEUDO_PREF = "devtools.inspector.show_pseudo_elements";

add_task(async function () {
  await pushPref(PSEUDO_PREF, true);
  await pushPref("dom.customHighlightAPI.enabled", true);

  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  await testTopLeft(inspector, view);
  await testTopRight(inspector, view);
  await testBottomRight(inspector, view);
  await testBottomLeft(inspector, view);
  await testParagraph(inspector, view);
  await testBody(inspector, view);
  await testList(inspector, view);
  await testDialogBackdrop(inspector, view);
  await testCustomHighlight(inspector, view);
});

async function testTopLeft(inspector, view) {
  const id = "#topleft";
  const rules = await assertPseudoElementRulesNumbers(id, inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 2,
    firstLetterRulesNb: 1,
    selectionRulesNb: 1,
    markerRulesNb: 0,
    afterRulesNb: 1,
    beforeRulesNb: 2,
  });

  const gutters = assertGutters(view);

  info("Make sure that clicking on the twisty hides pseudo elements");
  const expander = gutters[0].querySelector(".ruleview-expander");
  ok(!view.element.children[1].hidden, "Pseudo Elements are expanded");

  expander.click();
  ok(
    view.element.children[1].hidden,
    "Pseudo Elements are collapsed by twisty"
  );

  expander.click();
  ok(!view.element.children[1].hidden, "Pseudo Elements are expanded again");

  info(
    "Make sure that dblclicking on the header container also toggles " +
      "the pseudo elements"
  );
  EventUtils.synthesizeMouseAtCenter(
    gutters[0],
    { clickCount: 2 },
    view.styleWindow
  );
  ok(
    view.element.children[1].hidden,
    "Pseudo Elements are collapsed by dblclicking"
  );

  const elementRuleView = getRuleViewRuleEditor(view, 3);

  const elementFirstLineRule = rules.firstLineRules[0];
  const elementFirstLineRuleView = [
    ...view.element.children[1].children,
  ].filter(e => {
    return e._ruleEditor && e._ruleEditor.rule === elementFirstLineRule;
  })[0]._ruleEditor;

  is(
    convertTextPropsToString(elementFirstLineRule.textProps),
    "color: orange",
    "TopLeft firstLine properties are correct"
  );

  let onAdded = view.once("ruleview-changed");
  let firstProp = elementFirstLineRuleView.addProperty(
    "background-color",
    "rgb(0, 255, 0)",
    "",
    true
  );
  await onAdded;

  onAdded = view.once("ruleview-changed");
  const secondProp = elementFirstLineRuleView.addProperty(
    "font-style",
    "italic",
    "",
    true
  );
  await onAdded;

  is(
    firstProp,
    elementFirstLineRule.textProps[elementFirstLineRule.textProps.length - 2],
    "First added property is on back of array"
  );
  is(
    secondProp,
    elementFirstLineRule.textProps[elementFirstLineRule.textProps.length - 1],
    "Second added property is on back of array"
  );

  is(
    await getComputedStyleProperty(id, ":first-line", "background-color"),
    "rgb(0, 255, 0)",
    "Added property should have been used."
  );
  is(
    await getComputedStyleProperty(id, ":first-line", "font-style"),
    "italic",
    "Added property should have been used."
  );
  is(
    await getComputedStyleProperty(id, null, "text-decoration-line"),
    "none",
    "Added property should not apply to element"
  );

  await togglePropStatus(view, firstProp);

  is(
    await getComputedStyleProperty(id, ":first-line", "background-color"),
    "rgb(255, 0, 0)",
    "Disabled property should now have been used."
  );
  is(
    await getComputedStyleProperty(id, null, "background-color"),
    "rgb(221, 221, 221)",
    "Added property should not apply to element"
  );

  await togglePropStatus(view, firstProp);

  is(
    await getComputedStyleProperty(id, ":first-line", "background-color"),
    "rgb(0, 255, 0)",
    "Added property should have been used."
  );
  is(
    await getComputedStyleProperty(id, null, "text-decoration-line"),
    "none",
    "Added property should not apply to element"
  );

  onAdded = view.once("ruleview-changed");
  firstProp = elementRuleView.addProperty(
    "background-color",
    "rgb(0, 0, 255)",
    "",
    true
  );
  await onAdded;

  is(
    await getComputedStyleProperty(id, null, "background-color"),
    "rgb(0, 0, 255)",
    "Added property should have been used."
  );
  is(
    await getComputedStyleProperty(id, ":first-line", "background-color"),
    "rgb(0, 255, 0)",
    "Added prop does not apply to pseudo"
  );
}

async function testTopRight(inspector, view) {
  await assertPseudoElementRulesNumbers("#topright", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0,
    markerRulesNb: 0,
    beforeRulesNb: 2,
    afterRulesNb: 1,
  });

  const gutters = assertGutters(view);

  const expander = gutters[0].querySelector(".ruleview-expander");
  ok(
    !view.element.firstChild.classList.contains("show-expandable-container"),
    "Pseudo Elements remain collapsed after switching element"
  );

  expander.scrollIntoView();
  expander.click();
  ok(
    !view.element.children[1].hidden,
    "Pseudo Elements are shown again after clicking twisty"
  );
}

async function testBottomRight(inspector, view) {
  await assertPseudoElementRulesNumbers("#bottomright", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0,
    markerRulesNb: 0,
    beforeRulesNb: 3,
    afterRulesNb: 1,
  });
}

async function testBottomLeft(inspector, view) {
  await assertPseudoElementRulesNumbers("#bottomleft", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0,
    markerRulesNb: 0,
    beforeRulesNb: 2,
    afterRulesNb: 1,
  });
}

async function testParagraph(inspector, view) {
  const rules = await assertPseudoElementRulesNumbers(
    "#bottomleft p",
    inspector,
    view,
    {
      elementRulesNb: 3,
      firstLineRulesNb: 1,
      firstLetterRulesNb: 1,
      selectionRulesNb: 2,
      markerRulesNb: 0,
      beforeRulesNb: 0,
      afterRulesNb: 0,
    }
  );

  assertGutters(view);

  const elementFirstLineRule = rules.firstLineRules[0];
  is(
    convertTextPropsToString(elementFirstLineRule.textProps),
    "background: blue",
    "Paragraph first-line properties are correct"
  );

  const elementFirstLetterRule = rules.firstLetterRules[0];
  is(
    convertTextPropsToString(elementFirstLetterRule.textProps),
    "color: red; font-size: 130%",
    "Paragraph first-letter properties are correct"
  );

  const elementSelectionRule = rules.selectionRules[0];
  is(
    convertTextPropsToString(elementSelectionRule.textProps),
    "color: white; background: black",
    "Paragraph first-letter properties are correct"
  );
}

async function testBody(inspector, view) {
  await testNode("body", inspector, view);

  const gutters = getGutters(view);
  is(gutters.length, 0, "There are no gutter headings");
}

async function testList(inspector, view) {
  await assertPseudoElementRulesNumbers("#list", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0,
    markerRulesNb: 1,
    beforeRulesNb: 1,
    afterRulesNb: 1,
  });

  assertGutters(view);
}

async function testDialogBackdrop(inspector, view) {
  await assertPseudoElementRulesNumbers("dialog", inspector, view, {
    elementRulesNb: 3,
    backdropRules: 1,
  });

  assertGutters(view);
}

async function testCustomHighlight(inspector, view) {
  const { highlightRules } = await assertPseudoElementRulesNumbers(
    ".highlights-container",
    inspector,
    view,
    {
      elementRulesNb: 4,
      highlightRulesNb: 3,
    }
  );

  is(
    highlightRules[0].pseudoElement,
    "::highlight(filter)",
    "First highlight rule is for the filter highlight"
  );

  is(
    highlightRules[1].pseudoElement,
    "::highlight(search)",
    "Second highlight rule is for the search highlight"
  );
  is(
    highlightRules[2].pseudoElement,
    "::highlight(search)",
    "Third highlight rule is also for the search highlight"
  );
  is(highlightRules.length, 3, "Got all 3 active rules, but not unused one");

  // Check that properties are marked as overridden only when they're on the same Highlight
  is(
    convertTextPropsToString(highlightRules[0].textProps),
    `background-color: purple`,
    "Got expected properties for filter highlight"
  );
  is(
    convertTextPropsToString(highlightRules[1].textProps),
    `color: white`,
    "Got expected properties for first search highlight"
  );
  is(
    convertTextPropsToString(highlightRules[2].textProps),
    `background-color: tomato; ~~color: gold~~`,
    "Got expected properties for second search highlight, `color` is marked as overridden"
  );

  assertGutters(view);
}

function convertTextPropsToString(textProps) {
  return textProps
    .map(
      t =>
        `${t.overridden ? "~~" : ""}${t.name}: ${t.value}${
          t.overridden ? "~~" : ""
        }`
    )
    .join("; ");
}

async function testNode(selector, inspector, view) {
  await selectNode(selector, inspector);
  const elementStyle = view._elementStyle;
  return elementStyle;
}

async function assertPseudoElementRulesNumbers(
  selector,
  inspector,
  view,
  ruleNbs
) {
  const elementStyle = await testNode(selector, inspector, view);

  const rules = {
    elementRules: elementStyle.rules.filter(rule => !rule.pseudoElement),
    firstLineRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::first-line"
    ),
    firstLetterRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::first-letter"
    ),
    selectionRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::selection"
    ),
    markerRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::marker"
    ),
    beforeRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::before"
    ),
    afterRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::after"
    ),
    backdropRules: elementStyle.rules.filter(
      rule => rule.pseudoElement === "::backdrop"
    ),
    highlightRules: elementStyle.rules.filter(rule =>
      rule.pseudoElement?.startsWith("::highlight(")
    ),
  };

  is(
    rules.elementRules.length,
    ruleNbs.elementRulesNb || 0,
    selector + " has the correct number of non pseudo element rules"
  );
  is(
    rules.firstLineRules.length,
    ruleNbs.firstLineRulesNb || 0,
    selector + " has the correct number of ::first-line rules"
  );
  is(
    rules.firstLetterRules.length,
    ruleNbs.firstLetterRulesNb || 0,
    selector + " has the correct number of ::first-letter rules"
  );
  is(
    rules.selectionRules.length,
    ruleNbs.selectionRulesNb || 0,
    selector + " has the correct number of ::selection rules"
  );
  is(
    rules.markerRules.length,
    ruleNbs.markerRulesNb || 0,
    selector + " has the correct number of ::marker rules"
  );
  is(
    rules.beforeRules.length,
    ruleNbs.beforeRulesNb || 0,
    selector + " has the correct number of ::before rules"
  );
  is(
    rules.afterRules.length,
    ruleNbs.afterRulesNb || 0,
    selector + " has the correct number of ::after rules"
  );
  is(
    rules.highlightRules.length,
    ruleNbs.highlightRulesNb || 0,
    selector + " has the correct number of ::highlight rules"
  );

  // If we do have pseudo element rules displayed, ensure we don't mark their selectors
  // as matched or unmatched
  if (
    rules.elementRules.length &&
    elementStyle.rules.length !== rules.elementRules.length
  ) {
    const pseudoElementContainer = view.styleWindow.document.getElementById(
      "pseudo-elements-container"
    );
    const selectors = Array.from(
      pseudoElementContainer.querySelectorAll(".ruleview-selector")
    );
    ok(selectors.length, "We do have selectors for pseudo element rules");
    ok(
      selectors.every(
        selectorEl =>
          !selectorEl.classList.contains("matched") &&
          !selectorEl.classList.contains("unmatched")
      ),
      "Pseudo element selectors are not marked as matched nor unmatched"
    );
  }

  return rules;
}

function getGutters(view) {
  return view.element.querySelectorAll(".ruleview-header");
}

function assertGutters(view) {
  const gutters = getGutters(view);

  is(gutters.length, 3, "There are 3 gutter headings");
  is(gutters[0].textContent, "Pseudo-elements", "Gutter heading is correct");
  is(gutters[1].textContent, "This Element", "Gutter heading is correct");
  is(
    gutters[2].textContent,
    "Inherited from body",
    "Gutter heading is correct"
  );

  return gutters;
}
