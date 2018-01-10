/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a used font-family is highlighted in the rule-view.

const TEST_URI = `
  <style type="text/css">
    #id1 {
      font-family: foo, bar, sans-serif;
    }
    #id2 {
      font-family: serif;
    }
    #id3 {
      font-family: foo, monospace, monospace, serif;
    }
    #id4 {
      font-family: foo, bar;
    }
    #id5 {
      font-family: "monospace";
    }
  </style>
  <div id="id1">Text</div>
  <div id="id2">Text</div>
  <div id="id3">Text</div>
  <div id="id4">Text</div>
  <div id="id5">Text</div>
`;

// Tests that font-family properties in the rule-view correctly
// indicates which font is in use.
// Each entry in the test array should contain:
// {
//   selector: the rule-view selector to look for font-family in
//   nb: the number of fonts this property should have
//   used: the index of the font that should be highlighted, or
//         -1 if none should be highlighted
// }
const TESTS = [
  {selector: "#id1", nb: 3, used: 2}, // sans-serif
  {selector: "#id2", nb: 1, used: 0}, // serif
  {selector: "#id3", nb: 4, used: 1}, // monospace
  {selector: "#id4", nb: 2, used: -1},
  {selector: "#id5", nb: 1, used: 0}, // monospace
];

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  for (let {selector, nb, used} of TESTS) {
    let onFontHighlighted = view.once("font-highlighted");
    yield selectNode(selector, inspector);
    yield onFontHighlighted;

    info("Looking for fonts in font-family property in selector " + selector);

    let prop = getRuleViewProperty(view, selector, "font-family").valueSpan;
    let fonts = prop.querySelectorAll(".ruleview-font-family");

    ok(fonts.length, "Fonts found in the property");
    is(fonts.length, nb, "Correct number of fonts found in the property");

    const highlighted = [...fonts].filter(span => span.classList.contains("used-font"));

    ok(highlighted.length <= 1, "No more than one font highlighted");
    is([...fonts].findIndex(f => f === highlighted[0]), used, "Correct font highlighted");
  }
});
