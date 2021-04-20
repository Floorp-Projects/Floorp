/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that units are displayed correctly when editing values in the box model
// and that values are retrieved and parsed correctly from the back-end

const TEST_URI =
  "<style>" +
  "div { margin: 10px; padding: 3px }" +
  "#div1 { margin-top: 5px }" +
  "#div2 { border-bottom: 1em solid black; }" +
  "#div3 { padding: 2em; }" +
  "</style>" +
  "<div id='div1'></div><div id='div2'></div><div id='div3'></div>";

add_task(async function() {
  const tab = await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const browser = tab.linkedBrowser;
  const { inspector, boxmodel } = await openLayoutView();

  await testUnits(inspector, boxmodel, browser);
  await testValueComesFromStyleRule(inspector, boxmodel, browser);
  await testShorthandsAreParsed(inspector, boxmodel, browser);
});

async function testUnits(inspector, boxmodel, browser) {
  info("Test that entering units works");

  is(
    await getStyle(browser, "#div1", "padding-top"),
    "",
    "Should have the right padding"
  );
  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-top > span"
  );
  await waitForElementTextContent(span, "3");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "3px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("1", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);
  EventUtils.synthesizeKey("e", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(
    await getStyle(browser, "#div1", "padding-top"),
    "",
    "An invalid value is handled cleanly"
  );

  EventUtils.synthesizeKey("m", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "1em", "Should have the right value in the editor.");
  is(
    await getStyle(browser, "#div1", "padding-top"),
    "1em",
    "Should have updated the padding."
  );

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(browser, "#div1", "padding-top"),
    "1em",
    "Should be the right padding."
  );
  await waitForElementTextContent(span, "16");
}

async function testValueComesFromStyleRule(inspector, boxmodel, browser) {
  info("Test that we pick up the value from a higher style rule");

  is(
    await getStyle(browser, "#div2", "border-bottom-width"),
    "",
    "Should have the right border-bottom-width"
  );
  await selectNode("#div2", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-border.boxmodel-bottom > span"
  );
  await waitForElementTextContent(span, "16");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "1em", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("0", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "0", "Should have the right value in the editor.");
  is(
    await getStyle(browser, "#div2", "border-bottom-width"),
    "0px",
    "Should have updated the border."
  );

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(browser, "#div2", "border-bottom-width"),
    "0px",
    "Should be the right border-bottom-width."
  );
  await waitForElementTextContent(span, "0");
}

async function testShorthandsAreParsed(inspector, boxmodel, browser) {
  info("Test that shorthand properties are parsed correctly");

  is(
    await getStyle(browser, "#div3", "padding-right"),
    "",
    "Should have the right padding"
  );
  await selectNode("#div3", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-padding.boxmodel-right > span"
  );
  await waitForElementTextContent(span, "32");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "2em", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(browser, "#div3", "padding-right"),
    "",
    "Should be the right padding."
  );
  await waitForElementTextContent(span, "32");
}
