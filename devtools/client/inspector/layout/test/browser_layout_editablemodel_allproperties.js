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

add_task(function* () {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {inspector, view, testActor} = yield openLayoutView();

  yield testEditing(inspector, view, testActor);
  yield testEditingAndCanceling(inspector, view, testActor);
  yield testDeleting(inspector, view, testActor);
  yield testDeletingAndCanceling(inspector, view, testActor);
});

function* testEditing(inspector, view, testActor) {
  info("When all properties are set on the node editing one should work");

  yield setStyle(testActor, "#div1", "padding", "5px");
  yield waitForUpdate(inspector);

  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".layout-padding.layout-bottom > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("7", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "7", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "padding-bottom")), "7px",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is((yield getStyle(testActor, "#div1", "padding-bottom")), "7px",
     "Should be the right padding.");
  is(span.textContent, 7, "Should have the right value in the box model.");
}

function* testEditingAndCanceling(inspector, view, testActor) {
  info("When all properties are set on the node editing one and then " +
       "cancelling with ESCAPE should work");

  yield setStyle(testActor, "#div1", "padding", "5px");
  yield waitForUpdate(inspector);

  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".layout-padding.layout-left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("8", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "8", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "padding-left")), "8px",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is((yield getStyle(testActor, "#div1", "padding-left")), "5px",
     "Should be the right padding.");
  is(span.textContent, 5, "Should have the right value in the box model.");
}

function* testDeleting(inspector, view, testActor) {
  info("When all properties are set on the node deleting one should work");

  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".layout-padding.layout-left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "padding-left")), "",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is((yield getStyle(testActor, "#div1", "padding-left")), "",
     "Should be the right padding.");
  is(span.textContent, 3, "Should have the right value in the box model.");
}

function* testDeletingAndCanceling(inspector, view, testActor) {
  info("When all properties are set on the node deleting one then cancelling " +
       "should work");

  yield setStyle(testActor, "#div1", "padding", "5px");
  yield waitForUpdate(inspector);

  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".layout-padding.layout-left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is((yield getStyle(testActor, "#div1", "padding-left")), "",
     "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is((yield getStyle(testActor, "#div1", "padding-left")), "5px",
     "Should be the right padding.");
  is(span.textContent, 5, "Should have the right value in the box model.");
}
