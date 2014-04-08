/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that color swatches are displayed next to colors in the rule-view

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    color: red;',
  '    background-color: #ededed;',
  '    background-image: url(chrome://global/skin/icons/warning-64.png);',
  '    border: 2em solid rgba(120, 120, 120, .5);',
  '  }',
  '  * {',
  '    color: blue;',
  '    box-shadow: inset 0 0 2px 20px red, inset 0 0 2px 40px blue;',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

// Tests that properties in the rule-view contain color swatches
// Each entry in the test array should contain:
// {
//   selector: the rule-view selector to look for the property in
//   propertyName: the property to test
//   nb: the number of color swatches this property should have
// }
const TESTS = [
  {selector: "body", propertyName: "color", nb: 1},
  {selector: "body", propertyName: "background-color", nb: 1},
  {selector: "body", propertyName: "border", nb: 1},
  {selector: "*", propertyName: "color", nb: 1},
  {selector: "*", propertyName: "box-shadow", nb: 2},
];

let test = asyncTest(function*() {
  yield addTab("data:text/html,rule view color picker tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  for (let {selector, propertyName, nb} of TESTS) {
    info("Looking for color swatches in property " + propertyName +
      " in selector " + selector);

    let prop = getRuleViewProperty(view, selector, propertyName).valueSpan;
    let swatches = prop.querySelectorAll(".ruleview-colorswatch");

    ok(swatches.length, "Swatches found in the property");
    is(swatches.length, nb, "Correct number of swatches found in the property");
  }
});
