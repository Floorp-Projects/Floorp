/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing a mixed-case attribute preserves the case

const TEST_URL = TEST_URL_ROOT + "doc_markup_svg_attributes.html";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  yield inspector.markup.expandAll();
  yield selectNode("svg", inspector);

  yield testWellformedMixedCase(inspector);
  yield testMalformedMixedCase(inspector);
});

function* testWellformedMixedCase(inspector) {
  info("Modifying a mixed-case attribute, " +
    "expecting the attribute's case to be preserved");

  info("Listening to markup mutations");
  let onMutated = inspector.once("markupmutation");

  info("Focusing the viewBox attribute editor");
  let {editor} = yield getContainerForSelector("svg", inspector);
  let attr = editor.attrs["viewBox"].querySelector(".editable");
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  info("Editing the attribute value and waiting for the mutation event");
  let input = inplaceEditor(attr).input;
  input.value = "viewBox=\"0 0 1 1\"";
  EventUtils.sendKey("return", inspector.panelWin);
  yield onMutated;

  assertAttributes("svg", {
    "viewBox": "0 0 1 1",
    "width": "200",
    "height": "200"
  });
}

function* testMalformedMixedCase(inspector) {
  info("Modifying a mixed-case attribute, making sure to generate a parsing" +
    "error, and  expecting the attribute's case to NOT be preserved");
  // See /browser/devtools/markupview/markup-view.js:parseAttributeValues
  // When attributes are malformed, they cannot be parsed with the XML parser
  // and so we fall back to the HTML parser which lowercases attributes.

  info("Listening to markup mutations");
  let onMutated = inspector.once("markupmutation");

  info("Focusing the viewBox attribute editor");
  let {editor} = yield getContainerForSelector("svg", inspector);
  let attr = editor.attrs["viewBox"].querySelector(".editable");
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  info("Editing the attribute value and waiting for the mutation event");
  let input = inplaceEditor(attr).input;
  input.value = "viewBox=\"<>\"";
  EventUtils.sendKey("return", inspector.panelWin);
  yield onMutated;

  assertAttributes("svg", {
    "viewbox": "<>",
    "width": "200",
    "height": "200"
  });
}
