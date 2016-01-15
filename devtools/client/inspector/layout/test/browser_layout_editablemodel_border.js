/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing the border value in the box model applies the border style

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

add_task(function*() {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openLayoutView();

  let node = content.document.getElementById("div1");
  is(getStyle(node, "border-top-width"), "", "Should have the right border");
  is(getStyle(node, "border-top-style"), "", "Should have the right border");
  yield selectNode("#div1", inspector);

  let span = view.doc.querySelector(".border.top > span");
  is(span.textContent, 0, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");
  is(editor.value, "0", "Should have the right value in the editor.");

  EventUtils.synthesizeKey("1", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(editor.value, "1", "Should have the right value in the editor.");
  is(getStyle(node, "border-top-width"), "1px", "Should have the right border");
  is(getStyle(node, "border-top-style"), "solid", "Should have the right border");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);

  is(getStyle(node, "border-top-width"), "", "Should be the right padding.")
  is(getStyle(node, "border-top-style"), "", "Should have the right border");
  is(span.textContent, 0, "Should have the right value in the box model.");
});
