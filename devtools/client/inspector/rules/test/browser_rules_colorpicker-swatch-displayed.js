/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that color swatches are displayed next to colors in the rule-view.

const TEST_URI = `
  <style type="text/css">
    body {
      color: red;
      background-color: #ededed;
      background-image: url(chrome://branding/content/icon64.png);
      border: 2em solid rgba(120, 120, 120, .5);
    }
    * {
      color: blue;
      background: linear-gradient(
        to right,
        #f00,
        #f008,
        #00ff00,
        #00ff0080,
        rgb(31,170,217),
        rgba(31,170,217,.5),
        hsl(5, 5%, 5%),
        hsla(5, 5%, 5%, 0.25),
        #F00,
        #F008,
        #00FF00,
        #00FF0080,
        RGB(31,170,217),
        RGBA(31,170,217,.5),
        HSL(5, 5%, 5%),
        HSLA(5, 5%, 5%, 0.25));
      box-shadow: inset 0 0 2px 20px red, inset 0 0 2px 40px blue;
    }
  </style>
  Testing the color picker tooltip!
`;

// Tests that properties in the rule-view contain color swatches.
// Each entry in the test array should contain:
// {
//   selector: the rule-view selector to look for the property in
//   propertyName: the property to test
//   nb: the number of color swatches this property should have
// }
const TESTS = [
  { selector: "body", propertyName: "color", nb: 1 },
  { selector: "body", propertyName: "background-color", nb: 1 },
  { selector: "body", propertyName: "border", nb: 1 },
  { selector: "*", propertyName: "color", nb: 1 },
  { selector: "*", propertyName: "background", nb: 16 },
  { selector: "*", propertyName: "box-shadow", nb: 2 },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();

  for (const { selector, propertyName, nb } of TESTS) {
    info(
      "Looking for color swatches in property " +
        propertyName +
        " in selector " +
        selector
    );

    const prop = getRuleViewProperty(view, selector, propertyName).valueSpan;
    const swatches = prop.querySelectorAll(".ruleview-colorswatch");

    ok(swatches.length, "Swatches found in the property");
    is(swatches.length, nb, "Correct number of swatches found in the property");
  }
});
