/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* global getPropertyValue */

"use strict";

// Unit test for math behind conversion of units for letter-spacing.

const TEST_URI = `
  <style type='text/css'>
    body {
      /* Set root font-size to equivalent of 32px (2*16px) */
      font-size: 200%;
    }
    div {
      letter-spacing: 1em;
    }
  </style>
  <div>LETTER SPACING</div>
`;

add_task(async function() {
  const URI = "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI);
  const { inspector, view } = await openFontInspectorForURL(URI);
  const viewDoc = view.document;
  const property = "letter-spacing";
  const UNITS = {
    "px": 32,
    "rem": 2,
    "em": 1,
  };

  await selectNode("div", inspector);

  info("Check that font editor shows letter-spacing value in original units");
  const letterSpacing = getPropertyValue(viewDoc, property);
  is(letterSpacing.value + letterSpacing.unit, "1em", "Original letter spacing is 1em");

  // Starting value and unit for conversion.
  let prevValue = letterSpacing.value;
  let prevUnit = letterSpacing.unit;

  for (const unit in UNITS) {
    const value = UNITS[unit];

    info(`Convert letter-spacing from ${prevValue}${prevUnit} to ${unit}`);
    const convertedValue = await view.convertUnits(property, prevValue, prevUnit, unit);
    is(convertedValue, value, `Converting to ${unit} returns transformed value.`);

    // Store current unit and value to use in conversion on the next iteration.
    prevUnit = unit;
    prevValue = value;
  }

  info(`Check that conversion to fake unit returns 1-to-1 mapping`);
  const valueToFakeUnit = await view.convertUnits(property, 1, "px", "fake");
  is(valueToFakeUnit, 1, `Converting to fake unit returns same value.`);
});
