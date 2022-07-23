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
          HSLA(5, 5%, 5%, 0.25)
        ),
        radial-gradient(
          red,
          blue
        ),
        conic-gradient(
          from 90deg at 0 0,
          lemonchiffon,
          peachpuff
        ),
        repeating-linear-gradient(blue, pink),
        repeating-radial-gradient(limegreen, bisque),
        repeating-conic-gradient(chocolate, olive);
      box-shadow: inset 0 0 2px 20px red, inset 0 0 2px 40px blue;
      filter: drop-shadow(2px 2px 2px salmon);
      text-shadow: 2px 2px color-mix(
        in srgb,
        color-mix(in srgb, tomato 30%, #FA8072),
        hsla(5, 5%, 5%, 0.25) 5%
      );
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
  { selector: "*", propertyName: "background", nb: 26 },
  { selector: "*", propertyName: "box-shadow", nb: 2 },
  { selector: "*", propertyName: "filter", nb: 1 },
  { selector: "*", propertyName: "text-shadow", nb: 3 },
];

add_task(async function() {
  await pushPref("layout.css.color-mix.enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();

  for (const { selector, propertyName, nb } of TESTS) {
    info(
      "Looking for color swatches in property " +
        propertyName +
        " in selector " +
        selector
    );

    const prop = (
      await getRuleViewProperty(view, selector, propertyName, { wait: true })
    ).valueSpan;
    const swatches = prop.querySelectorAll(".ruleview-colorswatch");

    ok(swatches.length, "Swatches found in the property");
    is(swatches.length, nb, "Correct number of swatches found in the property");
  }
});
