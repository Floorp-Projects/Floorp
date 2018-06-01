/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pseudo elements have no side effect on the box model widget for their
// container. See bug 1350499.

const TEST_URI =
  `<style>
    .test::before {
      content: 'before';
      margin-top: 5px;
      padding-top: 5px;
      width: 5px;
    }
  </style>
  <div style='width:200px;'>
    <div class=test></div>
  </div>`;

add_task(async function() {
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const {inspector, boxmodel, testActor} = await openLayoutView();

  await selectNode(".test", inspector);

  // No margin-top defined.
  info("Test that margins are not impacted by a pseudo element");
  is((await getStyle(testActor, ".test", "margin-top")), "", "margin-top is correct");
  await checkValueInBoxModel(".boxmodel-margin.boxmodel-top", "0", boxmodel.document);

  // No padding-top defined.
  info("Test that paddings are not impacted by a pseudo element");
  is((await getStyle(testActor, ".test", "padding-top")), "", "padding-top is correct");
  await checkValueInBoxModel(".boxmodel-padding.boxmodel-top", "0", boxmodel.document);

  // Width should be driven by the parent div.
  info("Test that dimensions are not impacted by a pseudo element");
  is((await getStyle(testActor, ".test", "width")), "", "width is correct");
  await checkValueInBoxModel(".boxmodel-content.boxmodel-width", "200",
    boxmodel.document);
});

async function checkValueInBoxModel(selector, expectedValue, doc) {
  const span = doc.querySelector(selector + " > span");
  is(span.textContent, expectedValue, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, doc.defaultView);
  const editor = doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, expectedValue, "Should have the right value in the editor.");

  const onBlur = once(editor, "blur");
  EventUtils.synthesizeKey("VK_RETURN", {}, doc.defaultView);
  await onBlur;
}
