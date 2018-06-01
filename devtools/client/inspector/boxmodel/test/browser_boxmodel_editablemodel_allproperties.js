/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing box model values when all values are set

const TEST_URI = "<style>" +
  "div { margin: 10px; padding: 3px }" +
  "#div1 { margin-top: 5px }" +
  "#div2 { border-bottom: 1em solid black; }" +
  "#div3 { padding: 2em; }" +
  "</style>" +
  "<div id='div1'></div><div id='div2'></div><div id='div3'></div>";

add_task(async function() {
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const {inspector, boxmodel, testActor} = await openLayoutView();

  await testEditing(inspector, boxmodel, testActor);
  await testEditingAndCanceling(inspector, boxmodel, testActor);
  await testDeleting(inspector, boxmodel, testActor);
  await testDeletingAndCanceling(inspector, boxmodel, testActor);
});

async function testEditing(inspector, boxmodel, testActor) {
  info("When all properties are set on the node editing one should work");

  await setStyle(testActor, "#div1", "padding", "5px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span =
    boxmodel.document.querySelector(".boxmodel-padding.boxmodel-bottom > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("7", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "7", "Should have the right value in the editor.");
  is((await getStyle(testActor, "#div1", "padding-bottom")), "7px",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is((await getStyle(testActor, "#div1", "padding-bottom")), "7px",
     "Should be the right padding.");
  is(span.textContent, 7, "Should have the right value in the box model.");
}

async function testEditingAndCanceling(inspector, boxmodel, testActor) {
  info("When all properties are set on the node editing one and then " +
       "cancelling with ESCAPE should work");

  await setStyle(testActor, "#div1", "padding", "5px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(".boxmodel-padding.boxmodel-left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("8", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "8", "Should have the right value in the editor.");
  is((await getStyle(testActor, "#div1", "padding-left")), "8px",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is((await getStyle(testActor, "#div1", "padding-left")), "5px",
     "Should be the right padding.");
  is(span.textContent, 5, "Should have the right value in the box model.");
}

async function testDeleting(inspector, boxmodel, testActor) {
  info("When all properties are set on the node deleting one should work");

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(".boxmodel-padding.boxmodel-left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is((await getStyle(testActor, "#div1", "padding-left")), "",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  is((await getStyle(testActor, "#div1", "padding-left")), "",
     "Should be the right padding.");
  is(span.textContent, 3, "Should have the right value in the box model.");
}

async function testDeletingAndCanceling(inspector, boxmodel, testActor) {
  info("When all properties are set on the node deleting one then cancelling " +
       "should work");

  await setStyle(testActor, "#div1", "padding", "5px");
  await waitForUpdate(inspector);

  await selectNode("#div1", inspector);

  const span = boxmodel.document.querySelector(".boxmodel-padding.boxmodel-left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is((await getStyle(testActor, "#div1", "padding-left")), "",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);

  is((await getStyle(testActor, "#div1", "padding-left")), "5px",
     "Should be the right padding.");
  is(span.textContent, 5, "Should have the right value in the box model.");
}
