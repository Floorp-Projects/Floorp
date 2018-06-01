/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing a mixed-case attribute preserves the case

const TEST_URL = URL_ROOT + "doc_markup_svg_attributes.html";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);

  await inspector.markup.expandAll();
  await selectNode("svg", inspector);

  await testWellformedMixedCase(inspector, testActor);
  await testMalformedMixedCase(inspector, testActor);
});

async function testWellformedMixedCase(inspector, testActor) {
  info("Modifying a mixed-case attribute, " +
    "expecting the attribute's case to be preserved");

  info("Listening to markup mutations");
  const onMutated = inspector.once("markupmutation");

  info("Focusing the viewBox attribute editor");
  const {editor} = await focusNode("svg", inspector);
  const attr = editor.attrElements.get("viewBox").querySelector(".editable");
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  info("Editing the attribute value and waiting for the mutation event");
  const input = inplaceEditor(attr).input;
  input.value = "viewBox=\"0 0 1 1\"";
  EventUtils.sendKey("return", inspector.panelWin);
  await onMutated;

  await assertAttributes("svg", {
    "viewBox": "0 0 1 1",
    "width": "200",
    "height": "200"
  }, testActor);
}

async function testMalformedMixedCase(inspector, testActor) {
  info("Modifying a malformed, mixed-case attribute, " +
    "expecting the attribute's case to be preserved");

  info("Listening to markup mutations");
  const onMutated = inspector.once("markupmutation");

  info("Focusing the viewBox attribute editor");
  const {editor} = await focusNode("svg", inspector);
  const attr = editor.attrElements.get("viewBox").querySelector(".editable");
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  info("Editing the attribute value and waiting for the mutation event");
  const input = inplaceEditor(attr).input;
  input.value = "viewBox=\"<>\"";
  EventUtils.sendKey("return", inspector.panelWin);
  await onMutated;

  await assertAttributes("svg", {
    "viewBox": "<>",
    "width": "200",
    "height": "200"
  }, testActor);
}
