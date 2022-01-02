/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that inplace editors can be blurred by clicking outside of the editor.

const TEST_URI = `<style>
    #div1 {
      margin: 10px;
      padding: 3px;
    }
  </style>
  <div id="div1"></div>`;

add_task(async function() {
  // Make sure the toolbox is tall enough to have empty space below the
  // boxmodel-container.
  await pushPref("devtools.toolbox.footer.height", 500);

  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel } = await openLayoutView();

  await selectNode("#div1", inspector);
  await testClickingOutsideEditor(boxmodel);
  await testClickingBelowContainer(boxmodel);
});

async function testClickingOutsideEditor(boxmodel) {
  info("Test that clicking outside the editor blurs it");
  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );
  await waitForElementTextContent(span, "10");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");

  info("Click next to the opened editor input.");
  const onBlur = once(editor, "blur");
  const rect = editor.getBoundingClientRect();
  EventUtils.synthesizeMouse(
    editor,
    rect.width + 10,
    rect.height / 2,
    {},
    boxmodel.document.defaultView
  );
  await onBlur;

  is(
    boxmodel.document.querySelector(".styleinspector-propertyeditor"),
    null,
    "Inplace editor has been removed."
  );
}

async function testClickingBelowContainer(boxmodel) {
  info("Test that clicking below the box-model container blurs it");
  const span = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );
  await waitForElementTextContent(span, "10");

  info(
    "Test that clicking below the boxmodel-container blurs the opened editor"
  );
  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(
    ".styleinspector-propertyeditor"
  );
  ok(editor, "Should have opened the editor.");

  const onBlur = once(editor, "blur");
  const container = boxmodel.document.querySelector(".boxmodel-container");
  // Using getBoxQuads here because getBoundingClientRect (and therefore synthesizeMouse)
  // use an erroneous height of ~50px for the boxmodel-container.
  const bounds = container
    .getBoxQuads({ relativeTo: boxmodel.document })[0]
    .getBounds();
  EventUtils.synthesizeMouseAtPoint(
    bounds.left + 10,
    bounds.top + bounds.height + 10,
    {},
    boxmodel.document.defaultView
  );
  await onBlur;

  is(
    boxmodel.document.querySelector(".styleinspector-propertyeditor"),
    null,
    "Inplace editor has been removed."
  );
}
