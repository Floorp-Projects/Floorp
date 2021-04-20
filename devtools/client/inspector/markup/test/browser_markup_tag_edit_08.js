/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing various markup-containers' attribute fields, in particular
// attributes with long values and quotes

const TEST_URL = URL_ROOT + "doc_markup_edit.html";
/*eslint-disable */
const LONG_ATTRIBUTE =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const LONG_ATTRIBUTE_COLLAPSED =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEF\u2026UVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
/* eslint-enable */

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  await inspector.markup.expandAll();
  await testCollapsedLongAttribute(inspector, testActor);
  await testModifyInlineStyleWithQuotes(inspector, testActor);
  await testEditingAttributeWithMixedQuotes(inspector, testActor);
});

async function testCollapsedLongAttribute(inspector, testActor) {
  info("Try to modify the collapsed long attribute, making sure it expands.");

  info("Adding test attributes to the node");
  let onMutation = inspector.once("markupmutation");
  await setAttributeInBrowser(gBrowser.selectedBrowser, "#node24", "class", "");
  await onMutation;

  onMutation = inspector.once("markupmutation");
  await setAttributeInBrowser(
    gBrowser.selectedBrowser,
    "#node24",
    "data-long",
    LONG_ATTRIBUTE
  );
  await onMutation;

  await assertAttributes(
    "#node24",
    {
      id: "node24",
      class: "",
      "data-long": LONG_ATTRIBUTE,
    },
    testActor
  );

  const { editor } = await focusNode("#node24", inspector);
  const attr = editor.attrElements.get("data-long").querySelector(".editable");

  // Check to make sure it has expanded after focus
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  const input = inplaceEditor(attr).input;
  is(input.value, `data-long="${LONG_ATTRIBUTE}"`);
  EventUtils.sendKey("escape", inspector.panelWin);

  setEditableFieldValue(attr, input.value + ' data-short="ABC"', inspector);
  await inspector.once("markupmutation");

  const visibleAttrText = editor.attrElements
    .get("data-long")
    .querySelector(".attr-value").textContent;
  is(visibleAttrText, LONG_ATTRIBUTE_COLLAPSED);

  await assertAttributes(
    "#node24",
    {
      id: "node24",
      class: "",
      "data-long": LONG_ATTRIBUTE,
      "data-short": "ABC",
    },
    testActor
  );
}

async function testModifyInlineStyleWithQuotes(inspector, testActor) {
  info('Modify inline style containing "');

  await assertAttributes(
    "#node26",
    {
      id: "node26",
      style:
        'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F");',
    },
    testActor
  );

  const onMutated = inspector.once("markupmutation");
  const { editor } = await focusNode("#node26", inspector);
  const attr = editor.attrElements.get("style").querySelector(".editable");

  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  const input = inplaceEditor(attr).input;
  let value = input.value;

  is(
    value,
    "style='background-image: url(\"moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F\");'",
    "Value contains actual double quotes"
  );

  value = value.replace(/mozilla\.org/, "mozilla.com");
  input.value = value;

  EventUtils.sendKey("return", inspector.panelWin);

  await onMutated;

  await assertAttributes(
    "#node26",
    {
      id: "node26",
      style:
        'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.com%2F");',
    },
    testActor
  );
}

async function testEditingAttributeWithMixedQuotes(inspector, testActor) {
  info("Modify class containing \" and '");

  await assertAttributes(
    "#node27",
    {
      id: "node27",
      class: "Double \" and single '",
    },
    testActor
  );

  const onMutated = inspector.once("markupmutation");
  const { editor } = await focusNode("#node27", inspector);
  const attr = editor.attrElements.get("class").querySelector(".editable");

  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  const input = inplaceEditor(attr).input;
  let value = input.value;

  is(value, 'class="Double &quot; and single \'"', "Value contains &quot;");

  value = value.replace(/Double/, "&quot;").replace(/single/, "'");
  input.value = value;

  EventUtils.sendKey("return", inspector.panelWin);

  await onMutated;

  await assertAttributes(
    "#node27",
    {
      id: "node27",
      class: "\" \" and ' '",
    },
    testActor
  );
}
