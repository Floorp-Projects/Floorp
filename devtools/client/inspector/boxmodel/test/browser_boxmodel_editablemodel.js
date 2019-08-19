/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing the box-model values works as expected and test various
// key bindings

const TEST_URI =
  "<style>" +
  "div { margin: 10px; padding: 3px }" +
  "#div1 { margin-top: 5px }" +
  "#div2 { border-bottom: 1em solid black; }" +
  "#div3 { padding: 2em; }" +
  "#div4 { margin: 1px; }" +
  "</style>" +
  "<div id='div1'></div><div id='div2'></div>" +
  "<div id='div3'></div><div id='div4'></div>";

add_task(async function() {
  // Make sure the toolbox is tall enough to have empty space below the
  // boxmodel-container.
  await pushPref("devtools.toolbox.footer.height", 500);

  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel, testActor } = await openLayoutView();

  await testEditingMargins(inspector, boxmodel, testActor);
  await testKeyBindings(inspector, boxmodel, testActor);
  await testEscapeToUndo(inspector, boxmodel, testActor);
  await testDeletingValue(inspector, boxmodel, testActor);
  await testRefocusingOnClick(inspector, boxmodel, testActor);
});

async function testEditingMargins(inspector, boxmodel, testActor) {
  info(
    "Test that editing margin dynamically updates the document, pressing " +
      "escape cancels the changes"
  );

  is(
    await getStyle(testActor, "#div1", "margin-top"),
    "",
    "Should be no margin-top on the element."
  );
  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("3", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(
    await getStyle(testActor, "#div1", "margin-top"),
    "3px",
    "Should have updated the margin."
  );

  EventUtils.synthesizeKey("VK_ESCAPE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(
    await getStyle(testActor, "#div1", "margin-top"),
    "",
    "Should be no margin-top on the element."
  );
  is(span.textContent, 5, "Should have the right value in the box model.");
}

async function testKeyBindings(inspector, boxmodel, testActor) {
  info(
    "Test that arrow keys work correctly and pressing enter commits the " +
      "changes"
  );

  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "",
    "Should be no margin-top on the element."
  );
  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-left > span"
  );
  is(span.textContent, 10, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "10px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_UP", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "11px", "Should have the right value in the editor.");
  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "11px",
    "Should have updated the margin."
  );

  EventUtils.synthesizeKey("VK_DOWN", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "10px", "Should have the right value in the editor.");
  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "10px",
    "Should have updated the margin."
  );

  EventUtils.synthesizeKey(
    "VK_UP",
    { shiftKey: true },
    boxmodel.document.defaultView
  );
  await waitForUpdate(inspector);

  is(editor.value, "20px", "Should have the right value in the editor.");
  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "20px",
    "Should have updated the margin."
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "20px",
    "Should be the right margin-top on the element."
  );
  is(span.textContent, 20, "Should have the right value in the box model.");
}

async function testEscapeToUndo(inspector, boxmodel, testActor) {
  info(
    "Test that deleting the value removes the property but escape undoes " +
      "that"
  );

  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "20px",
    "Should be the right margin-top on the element."
  );
  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-left > span"
  );
  is(span.textContent, 20, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "20px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "",
    "Should have updated the margin."
  );

  EventUtils.synthesizeKey("VK_ESCAPE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(
    await getStyle(testActor, "#div1", "margin-left"),
    "20px",
    "Should be the right margin-top on the element."
  );
  is(span.textContent, 20, "Should have the right value in the box model.");
}

async function testDeletingValue(inspector, boxmodel, testActor) {
  info("Test that deleting the value removes the property");

  await setStyle(testActor, "#div1", "marginRight", "15px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-right > span"
  );
  is(span.textContent, 15, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");
  is(editor.value, "15px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(
    await getStyle(testActor, "#div1", "margin-right"),
    "",
    "Should have updated the margin."
  );

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(testActor, "#div1", "margin-right"),
    "",
    "Should be the right margin-top on the element."
  );
  is(span.textContent, 10, "Should have the right value in the box model.");
}

async function testRefocusingOnClick(inspector, boxmodel, testActor) {
  info("Test that clicking in the editor input does not remove focus");

  await selectNode("#div4", inspector);

  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );
  is(span.textContent, 1, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");

  info("Click in the already opened editor input");
  EventUtils.synthesizeMouseAtCenter(editor, {}, boxmodel.document.defaultView);
  is(
    editor,
    boxmodel.document.activeElement,
    "Inplace editor input should still have focus."
  );

  info("Check the input can still be used as expected");
  EventUtils.synthesizeKey("VK_UP", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "2px", "Should have the right value in the editor.");
  is(
    await getStyle(testActor, "#div4", "margin-top"),
    "2px",
    "Should have updated the margin."
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is(
    await getStyle(testActor, "#div4", "margin-top"),
    "2px",
    "Should be the right margin-top on the element."
  );
  is(span.textContent, 2, "Should have the right value in the box model.");
}
