/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the inspector links in the webconsole output for namespaced elements
// actually open the inspector and select the right node.

const XHTML = `
  <!DOCTYPE html>
  <html xmlns="http://www.w3.org/1999/xhtml"
        xmlns:svg="http://www.w3.org/2000/svg">
    <body>
      <svg:svg width="100" height="100">
        <svg:clipPath id="clip">
          <svg:rect id="rectangle" x="0" y="0" width="10" height="5"></svg:rect>
        </svg:clipPath>
        <svg:circle cx="0" cy="0" r="5"></svg:circle>
      </svg:svg>
    </body>
  </html>
`;

const TEST_URI = "data:application/xhtml+xml;charset=utf-8," + encodeURI(XHTML);

const TEST_DATA = [
  {
    input: 'document.querySelector("clipPath")',
    output: '<svg:clipPath id="clip">',
    displayName: "svg:clipPath"
  },
  {
    input: 'document.querySelector("circle")',
    output: '<svg:circle cx="0" cy="0" r="5">',
    displayName: "svg:circle"
  },
];

function test() {
  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    yield checkDomElementHighlightingForInputs(hud, TEST_DATA);
  }).then(finishTest);
}
