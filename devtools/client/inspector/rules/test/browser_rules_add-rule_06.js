/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule using the add rule button
// on namespaced elements.

const XHTML = `
  <!DOCTYPE html>
  <html xmlns="http://www.w3.org/1999/xhtml"
        xmlns:svg="http://www.w3.org/2000/svg">
    <body>
      <svg:svg width="100" height="100">
        <svg:clipPath>
          <svg:rect x="0" y="0" width="10" height="5"></svg:rect>
        </svg:clipPath>
        <svg:circle cx="0" cy="0" r="5"></svg:circle>
      </svg:svg>
    </body>
  </html>
`;
const TEST_URI = "data:application/xhtml+xml;charset=utf-8," + encodeURI(XHTML);

const TEST_DATA = [
  { node: "clipPath", expected: "clipPath" },
  { node: "rect", expected: "rect" },
  { node: "circle", expected: "circle" }
];

add_task(function* () {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();

  for (let data of TEST_DATA) {
    let {node, expected} = data;
    yield selectNode(node, inspector);
    yield addNewRule(inspector, view);
    yield testNewRule(view, expected, 1);
  }
});

function* testNewRule(view, expected, index) {
  let idRuleEditor = getRuleViewRuleEditor(view, index);
  let editor = idRuleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, expected,
      "Selector editor value is as expected: " + expected);
}
