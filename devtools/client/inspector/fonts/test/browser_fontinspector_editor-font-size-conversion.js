/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Unit test for math behind conversion of units for font-size. A reference element is
// needed for converting to and from relative units (rem, em, %). A controlled viewport
// is needed (iframe) for converting to and from viewport units (vh, vw, vmax, vmin).

const TEST_URI = URL_ROOT + "doc_browser_fontinspector_iframe.html";

add_task(async function() {
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  const property = "font-size";
  const selector = ".viewport-size";
  const UNITS = {
    px: 50,
    vw: 10,
    vh: 20,
    vmin: 20,
    vmax: 10,
    em: 1.389,
    rem: 3.125,
    "%": 138.889,
    pt: 37.5,
    pc: 3.125,
    mm: 13.229,
    cm: 1.323,
    in: 0.521,
  };

  const node = await getNodeFrontInFrame(selector, "#frame", inspector);
  await selectNode(node, inspector);

  info("Check that font editor shows font-size value in original units");
  const fontSize = getPropertyValue(viewDoc, property);
  is(fontSize.unit, "vw", "Original unit for font size is vw");
  is(fontSize.value + fontSize.unit, "10vw", "Original font size is 10vw");

  // Starting value and unit for conversion.
  let prevValue = fontSize.value;
  let prevUnit = fontSize.unit;

  for (const unit in UNITS) {
    const value = UNITS[unit];

    info(`Convert font-size from ${prevValue}${prevUnit} to ${unit}`);
    const convertedValue = await view.convertUnits(
      property,
      prevValue,
      prevUnit,
      unit
    );
    is(
      convertedValue,
      value,
      `Converting to ${unit} returns transformed value.`
    );

    // Store current unit and value to use in conversion on the next iteration.
    prevUnit = unit;
    prevValue = value;
  }

  info(`Check that conversion from fake unit returns 1-to-1 mapping.`);
  const valueFromFakeUnit = await view.convertUnits(property, 1, "fake", "px");
  is(valueFromFakeUnit, 1, `Converting from fake unit returns same value.`);

  info(`Check that conversion to fake unit returns 1-to-1 mapping`);
  const valueToFakeUnit = await view.convertUnits(property, 1, "px", "fake");
  is(valueToFakeUnit, 1, `Converting to fake unit returns same value.`);

  info(`Check that conversion between fake units returns 1-to-1 mapping.`);
  const valueBetweenFakeUnit = await view.convertUnits(
    property,
    1,
    "bogus",
    "fake"
  );
  is(
    valueBetweenFakeUnit,
    1,
    `Converting between fake units returns same value.`
  );
});
