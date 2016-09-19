/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing the box-model values works as expected and test various
// key bindings

const TEST_URI = "<style>" +
  "div { margin: 10px; padding: 3px }" +
  "#div1 { margin-top: 5px }" +
  "#div2 { border-bottom: 1em solid black; }" +
  "#div3 { padding: 2em; }" +
  "#div4 { margin: 1px; }" +
  "</style>" +
  "<div id='div1'></div><div id='div2'></div>" +
  "<div id='div3'></div><div id='div4'></div>";

add_task(function* () {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {inspector, view, testActor} = yield openBoxModelView();

  yield testEditingMargins(inspector, view, testActor);
  yield testKeyBindings(inspector, view, testActor);
  yield testEscapeToUndo(inspector, view, testActor);
  yield testDeletingValue(inspector, view, testActor);
  yield testRefocusingOnClick(inspector, view, testActor);
});

function* testEditingMargins(inspector, view, testActor) {
  info("Test that editing margin dynamically updates the document, pressing " +
       "escape cancels the changes");

  is((yield getStyle(testActor, "#div1", "margin-top")), "",
     "Should be no margin-top on the element.");
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-top > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("3", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is((yield getStyle(testActor, "#div1", "margin-top")), "3px",
     "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is((yield getStyle(testActor, "#div1", "margin-top")), "",
     "Should be no margin-top on the element.");
  is(span.textContent, 5, "Should have the right value in the box model.");
}

function* testKeyBindings(inspector, view, testActor) {
  info("Test that arrow keys work correctly and pressing enter commits the " +
       "changes");

  is((yield getStyle(testActor, "#div1", "margin-left")), "",
     "Should be no margin-top on the element.");
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-left > span");
  is(span.textContent, 10, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "10px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_UP", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "11px", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "margin-left")), "11px",
     "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_DOWN", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "10px", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "margin-left")), "10px",
     "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_UP", { shiftKey: true }, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "20px", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "margin-left")), "20px",
     "Should have updated the margin.");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is((yield getStyle(testActor, "#div1", "margin-left")), "20px",
     "Should be the right margin-top on the element.");
  is(span.textContent, 20, "Should have the right value in the box model.");
}

function* testEscapeToUndo(inspector, view, testActor) {
  info("Test that deleting the value removes the property but escape undoes " +
       "that");

  is((yield getStyle(testActor, "#div1", "margin-left")), "20px",
     "Should be the right margin-top on the element.");
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-left > span");
  is(span.textContent, 20, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "20px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "margin-left")), "",
     "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is((yield getStyle(testActor, "#div1", "margin-left")), "20px",
     "Should be the right margin-top on the element.");
  is(span.textContent, 20, "Should have the right value in the box model.");
}

function* testDeletingValue(inspector, view, testActor) {
  info("Test that deleting the value removes the property");

  yield setStyle(testActor, "#div1", "marginRight", "15px");
  yield waitForUpdate(inspector);

  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-right > span");
  is(span.textContent, 15, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "15px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "margin-right")), "",
     "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is((yield getStyle(testActor, "#div1", "margin-right")), "",
     "Should be the right margin-top on the element.");
  is(span.textContent, 10, "Should have the right value in the box model.");
}

function* testRefocusingOnClick(inspector, view, testActor) {
  info("Test that clicking in the editor input does not remove focus");

  yield selectNode("#div4", inspector);

  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-top > span");
  is(span.textContent, 1, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");

  info("Click in the already opened editor input");
  EventUtils.synthesizeMouseAtCenter(editor, {}, view.doc.defaultView);
  is(editor, view.doc.activeElement,
    "Inplace editor input should still have focus.");

  info("Check the input can still be used as expected");
  EventUtils.synthesizeKey("VK_UP", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "2px", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div4", "margin-top")), "2px",
     "Should have updated the margin.");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is((yield getStyle(testActor, "#div4", "margin-top")), "2px",
     "Should be the right margin-top on the element.");
  is(span.textContent, 2, "Should have the right value in the box model.");
}
