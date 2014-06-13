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

function getStyle(node, property) {
  return node.style.getPropertyValue(property);
}

let test = asyncTest(function*() {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openLayoutView();

  yield runTests(inspector, view);
  yield destroyToolbox(inspector);
});

addTest("When all properties are set on the node editing one should work",
function*(inspector, view) {
  let node = content.document.getElementById("div1");

  node.style.padding = "5px";
  yield waitForUpdate(inspector);

  yield selectNode(node, inspector);

  let span = view.doc.querySelector(".padding.bottom > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("7", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "7", "Should have the right value in the editor.");
  is(getStyle(node, "padding-bottom"), "7px", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(getStyle(node, "padding-bottom"), "7px", "Should be the right padding.")
  is(span.textContent, 7, "Should have the right value in the box model.");
});

addTest("When all properties are set on the node editing one should work",
function*(inspector, view) {
  let node = content.document.getElementById("div1");

  node.style.padding = "5px";
  yield waitForUpdate(inspector);

  yield selectNode(node, inspector);

  let span = view.doc.querySelector(".padding.left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("8", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "8", "Should have the right value in the editor.");
  is(getStyle(node, "padding-left"), "8px", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(getStyle(node, "padding-left"), "5px", "Should be the right padding.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});

addTest("When all properties are set on the node deleting one should work",
function*(inspector, view) {
  let node = content.document.getElementById("div1");

  node.style.padding = "5px";

  yield selectNode(node, inspector);

  let span = view.doc.querySelector(".padding.left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "padding-left"), "", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(getStyle(node, "padding-left"), "", "Should be the right padding.")
  is(span.textContent, 3, "Should have the right value in the box model.");
});

addTest("When all properties are set on the node deleting one then cancelling should work",
function*(inspector, view) {
  let node = content.document.getElementById("div1");

  node.style.padding = "5px";
  yield waitForUpdate(inspector);

  yield selectNode(node, inspector);

  let span = view.doc.querySelector(".padding.left > span");
  is(span.textContent, 5, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "5px", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("VK_DELETE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "", "Should have the right value in the editor.");
  is(getStyle(node, "padding-left"), "", "Should have updated the padding");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(getStyle(node, "padding-left"), "5px", "Should be the right padding.")
  is(span.textContent, 5, "Should have the right value in the box model.");
});
