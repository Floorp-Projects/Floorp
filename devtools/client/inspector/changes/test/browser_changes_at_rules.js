/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Changes panel works with nested at-rules.

// Declare rule individually so we can use them for the assertions as well
// In the end, we should have nested rule looking like:
// - @media screen and (height > 5px) {
// -- @layer myLayer {
// --- @container myContainer (width > 10px) {
// ----- div {

const divRule = `div {
  color: tomato;
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
  <div>hello</div>
`;

const EXPECTED = [
  {
    text: "@media screen and (height > 5px) {",
    copyRuleClipboard: mediaRule.replace("tomato", "cyan"),
  },
  {
    text: "@layer myLayer {",
    copyRuleClipboard: layerRule.replace("tomato", "cyan"),
  },
  {
    text: "@container myContainer (width > 10px) {",
    copyRuleClipboard: containerRule.replace("tomato", "cyan"),
  },
  { text: "div {", copyRuleClipboard: divRule.replace("tomato", "cyan") },
];

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const changesView = selectChangesView(inspector);
  const { document: panelDoc, store } = changesView;
  const panel = panelDoc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  const onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await updateDeclaration(ruleView, 1, { color: "tomato" }, { color: "cyan" });
  await onTrackChange;

  const selectorsEl = getSelectors(panel);

  is(
    selectorsEl.length,
    EXPECTED.length,
    "Got the expected number of selectors item"
  );
  for (let i = 0; i < EXPECTED.length; i++) {
    const selectorEl = selectorsEl[i];
    const expectedItem = EXPECTED[i];
    is(
      selectorEl.text,
      expectedItem.innerText,
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
});

function checkClipboardData(expected) {
  const actual = SpecialPowers.getClipboardData("text/plain");
  return actual.trim() === expected.trim();
}
