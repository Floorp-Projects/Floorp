/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that inplace editors can be blurred by clicking outside of the editor.

const TEST_URI =
  `<style>
    #div1 {
      margin: 10px;
      padding: 3px;
    }
  </style>
  <div id="div1"></div>`;

add_task(function* () {
  // Make sure the toolbox is tall enough to have empty space below the
  // boxmodel-container.
  yield pushPref("devtools.toolbox.footer.height", 500);

  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openBoxModelView();

  yield selectNode("#div1", inspector);
  yield testClickingOutsideEditor(view);
  yield testClickingBelowContainer(view);
});

function* testClickingOutsideEditor(view) {
  info("Test that clicking outside the editor blurs it");
  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-top > span");
  is(span.textContent, 10, "Should have the right value in the box model.");

  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");

  info("Click next to the opened editor input.");
  let onBlur = once(editor, "blur");
  let rect = editor.getBoundingClientRect();
  EventUtils.synthesizeMouse(editor, rect.width + 10, rect.height / 2, {},
    view.doc.defaultView);
  yield onBlur;

  is(view.doc.querySelector(".styleinspector-propertyeditor"), null,
    "Inplace editor has been removed.");
}

function* testClickingBelowContainer(view) {
  info("Test that clicking below the box-model container blurs it");
  let span = view.doc.querySelector(".boxmodel-margin.boxmodel-top > span");
  is(span.textContent, 10, "Should have the right value in the box model.");

  info("Test that clicking below the boxmodel-container blurs the opened editor");
  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");
  ok(editor, "Should have opened the editor.");

  let onBlur = once(editor, "blur");
  let container = view.doc.querySelector("#boxmodel-container");
  // Using getBoxQuads here because getBoundingClientRect (and therefore synthesizeMouse)
  // use an erroneous height of ~50px for the boxmodel-container.
  let bounds = container.getBoxQuads({relativeTo: view.doc})[0].bounds;
  EventUtils.synthesizeMouseAtPoint(
    bounds.left + 10,
    bounds.top + bounds.height + 10,
    {}, view.doc.defaultView);
  yield onBlur;

  is(view.doc.querySelector(".styleinspector-propertyeditor"), null,
    "Inplace editor has been removed.");
}
