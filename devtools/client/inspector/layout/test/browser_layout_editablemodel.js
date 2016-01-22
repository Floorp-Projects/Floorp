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

function getStyle(node, property) {
  return node.style.getPropertyValue(property);
}

add_task(function*() {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openLayoutView();

  yield runTests(inspector, view);
});

addTest("Test that editing margin dynamically updates the document, pressing escape cancels the changes",
function*(inspector, view) {
  let node = content.document.getElementById("div1");
  is(getStyle(node, "margin-top"), "", "Should be no margin-top on the element.")
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".margin.top > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("3", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(getStyle(node, "margin-top"), "3px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(getStyle(node, "margin-top"), "", "Should be no margin-top on the element.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});

addTest("Test that arrow keys work correctly and pressing enter commits the changes",
function*(inspector, view) {
  let node = content.document.getElementById("div1");
  is(getStyle(node, "margin-left"), "", "Should be no margin-top on the element.")
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".margin.left > span");
  is(span.textContent, 10, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "10px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_UP", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "11px", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "11px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_DOWN", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "10px", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "10px", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_UP", { shiftKey: true }, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "20px", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "20px", "Should have updated the margin.");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(getStyle(node, "margin-left"), "20px", "Should be the right margin-top on the element.")
  is(span.textContent, 20, "Should have the right value in the box model.");
});

addTest("Test that deleting the value removes the property but escape undoes that",
function*(inspector, view) {
  let node = content.document.getElementById("div1");
  is(getStyle(node, "margin-left"), "20px", "Should be the right margin-top on the element.")
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".margin.left > span");
  is(span.textContent, 20, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "20px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "margin-left"), "", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(getStyle(node, "margin-left"), "20px", "Should be the right margin-top on the element.")
  is(span.textContent, 20, "Should have the right value in the box model.");
});

addTest("Test that deleting the value removes the property",
function*(inspector, view) {
  let node = content.document.getElementById("div1");

  node.style.marginRight = "15px";
  yield waitForUpdate(inspector);

  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".margin.right > span");
  is(span.textContent, 15, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "15px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "margin-right"), "", "Should have updated the margin.");

  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(getStyle(node, "margin-right"), "", "Should be the right margin-top on the element.")
  is(span.textContent, 10, "Should have the right value in the box model.");
});

addTest("Test that clicking in the editor input does not remove focus",
function*(inspector, view) {
  let node = content.document.getElementById("div4");

  yield selectNode("#div4", inspector);

  let span = view.doc.querySelector(".margin.top > span");
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
  is(getStyle(node, "margin-top"), "2px", "Should have updated the margin.");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(getStyle(node, "margin-top"), "2px",
    "Should be the right margin-top on the element.");
  is(span.textContent, 2, "Should have the right value in the box model.");
});
