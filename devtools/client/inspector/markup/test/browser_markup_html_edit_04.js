/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that outerHTML editing keybindings work as expected and that the <svg>
// root element can be edited correctly.

const TEST_URL =
  "data:image/svg+xml," +
  '<svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">' +
  '<circle cx="50" cy="50" r="50"/>' +
  "</svg>";

requestLongerTimeout(2);

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  inspector.markup._frame.focus();

  info("Check that editing the <svg> element works like other nodes");
  await testDocumentElement(inspector, testActor);

  info("Check (again) that editing the <svg> element works like other nodes");
  await testDocumentElement2(inspector, testActor);
});

async function testDocumentElement(inspector, testActor) {
  const currentDocElementOuterHTML = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.documentElement.outerHTML
  );
  const docElementSVG =
    '<svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">' +
    '<rect x="10" y="10" width="180" height="180"/>' +
    "</svg>";
  const docElementFront = await inspector.markup.walker.documentElement();

  const onReselected = inspector.markup.once("reselectedonremoved");
  await inspector.markup.updateNodeOuterHTML(
    docElementFront,
    docElementSVG,
    currentDocElementOuterHTML
  );
  await onReselected;

  is(
    await getAttributeInBrowser(gBrowser.selectedBrowser, "svg", "width"),
    "200",
    "<svg> width has been updated"
  );
  is(
    await getAttributeInBrowser(gBrowser.selectedBrowser, "svg", "height"),
    "200",
    "<svg> height has been updated"
  );
  is(
    await testActor.getProperty("svg", "outerHTML"),
    docElementSVG,
    "<svg> markup has been updated"
  );
}

async function testDocumentElement2(inspector, testActor) {
  const currentDocElementOuterHTML = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.documentElement.outerHTML
  );
  const docElementSVG =
    '<svg width="300" height="300" xmlns="http://www.w3.org/2000/svg">' +
    '<ellipse cx="150" cy="150" rx="150" ry="100"/>' +
    "</svg>";
  const docElementFront = await inspector.markup.walker.documentElement();

  const onReselected = inspector.markup.once("reselectedonremoved");
  inspector.markup.updateNodeOuterHTML(
    docElementFront,
    docElementSVG,
    currentDocElementOuterHTML
  );
  await onReselected;

  is(
    await getAttributeInBrowser(gBrowser.selectedBrowser, "svg", "width"),
    "300",
    "<svg> width has been updated"
  );
  is(
    await getAttributeInBrowser(gBrowser.selectedBrowser, "svg", "height"),
    "300",
    "<svg> height has been updated"
  );
  is(
    await testActor.getProperty("svg", "outerHTML"),
    docElementSVG,
    "<svg> markup has been updated"
  );
}
