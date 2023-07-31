/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Changes panel works with nested rules.

// Declare rule individually so we can use them for the assertions as well
// In the end, we should have nested rule looking like:
// - @media screen and (height > 5px) {
// -- @layer myLayer {
// --- @container myContainer (width > 10px) {
// ----- div {
// ------- & > span { … }
// ------- .mySpan {
// --------- &:not(:focus) {

const spanNotFocusedRule = `&:not(:focus) {
  text-decoration: underline;
}`;

const spanClassRule = `.mySpan {
  font-weight: bold;
  ${spanNotFocusedRule}
}`;

const spanRule = `& > span {
  outline: 1px solid gold;
}`;

const divRule = `div {
  color: tomato;
  ${spanRule}
  ${spanClassRule}
}`;

const containerRule = `@container myContainer (width > 10px) {
  /* in container */
  ${divRule}
}`;
const layerRule = `@layer myLayer {
  /* in layer */
  ${containerRule}
}`;
const mediaRule = `@media screen and (height > 5px) {
  /* in media */
    ${layerRule}
}`;

const TEST_URI = `
  <style>
  body {
    container: myContainer / inline-size
  }
  ${mediaRule}
  </style>
  <div>hello <span class="mySpan">world</span></div>
`;

const applyModificationAfterDivPropertyChange = ruleText =>
  ruleText.replace("tomato", "cyan");

const EXPECTED_AFTER_DIV_PROP_CHANGE = [
  {
    text: "@media screen and (height > 5px) {",
    copyRuleClipboard: applyModificationAfterDivPropertyChange(mediaRule),
  },
  {
    text: "@layer myLayer {",
    copyRuleClipboard: applyModificationAfterDivPropertyChange(layerRule),
  },
  {
    text: "@container myContainer (width > 10px) {",
    copyRuleClipboard: applyModificationAfterDivPropertyChange(containerRule),
  },
  {
    text: "div {",
    copyRuleClipboard: applyModificationAfterDivPropertyChange(divRule),
  },
];

const applyModificationAfterSpanPropertiesChange = ruleText =>
  ruleText
    .replace("1px solid gold", "4px solid gold")
    .replace("bold", "bolder")
    .replace("underline", "underline dotted");

const EXPECTED_AFTER_SPAN_PROP_CHANGES = EXPECTED_AFTER_DIV_PROP_CHANGE.map(
  expected => ({
    ...expected,
    copyRuleClipboard: applyModificationAfterSpanPropertiesChange(
      expected.copyRuleClipboard
    ),
  })
).concat([
  {
    text: ".mySpan {",
    copyRuleClipboard:
      applyModificationAfterSpanPropertiesChange(spanClassRule),
  },
  {
    text: "&:not(:focus) {",
    copyRuleClipboard:
      applyModificationAfterSpanPropertiesChange(spanNotFocusedRule),
  },
  {
    text: "& > span {",
    copyRuleClipboard: applyModificationAfterSpanPropertiesChange(spanRule),
  },
]);

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const changesView = selectChangesView(inspector);
  const { document: panelDoc, store } = changesView;
  const panel = panelDoc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  let onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await updateDeclaration(ruleView, 1, { color: "tomato" }, { color: "cyan" });
  await onTrackChange;

  await assertSelectors(panel, EXPECTED_AFTER_DIV_PROP_CHANGE);

  await selectNode(".mySpan", inspector);
  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await updateDeclaration(
    ruleView,
    1,
    { "text-decoration": "underline" },
    { "text-decoration": "underline dotted" }
  );
  await onTrackChange;

  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await updateDeclaration(
    ruleView,
    2,
    { "font-weight": "bold" },
    { "font-weight": "bolder" }
  );
  await onTrackChange;

  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await updateDeclaration(
    ruleView,
    3,
    { outline: "1px solid gold" },
    { outline: "4px solid gold" }
  );
  await onTrackChange;

  await assertSelectors(panel, EXPECTED_AFTER_SPAN_PROP_CHANGES);
});

async function assertSelectors(panel, expected) {
  const selectorsEl = getSelectors(panel);

  is(
    selectorsEl.length,
    expected.length,
    "Got the expected number of selectors item"
  );

  for (let i = 0; i < expected.length; i++) {
    const selectorEl = selectorsEl[i];
    const expectedItem = expected[i];

    is(
      selectorEl.innerText,
      expectedItem.text,
      `Got expected selector text at index ${i}`
    );
    info(`Click the Copy Rule button for the "${expectedItem.text}" rule`);
    const button = selectorEl
      .closest(".changes__rule")
      .querySelector(".changes__copy-rule-button");
    await waitForClipboardPromise(
      () => button.click(),
      () => checkClipboardData(expectedItem.copyRuleClipboard)
    );
  }
}

function checkClipboardData(expected) {
  const actual = SpecialPowers.getClipboardData("text/plain");
  return actual.trim() === expected.trim();
}
