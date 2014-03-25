/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing various markup-containers' attribute fields, in particular
// attributes with long values and quotes

const TEST_URL = TEST_URL_ROOT + "doc_markup_edit.html";
const LONG_ATTRIBUTE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const LONG_ATTRIBUTE_COLLAPSED = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEF\u2026UVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUVWXYZ";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  yield inspector.markup.expandAll();
  yield testCollapsedLongAttribute(inspector);
  yield testModifyInlineStyleWithQuotes(inspector);
  yield testEditingAttributeWithMixedQuotes(inspector);
});

function* testCollapsedLongAttribute(inspector) {
  info("Try to modify the collapsed long attribute, making sure it expands.");

  info("Adding test attributes to the node");
  let onMutated = inspector.once("markupmutation");
  let node = getNode("#node24");
  node.setAttribute("class", "");
  node.setAttribute("data-long", LONG_ATTRIBUTE);
  yield onMutated;

  assertAttributes("#node24", {
    id: "node24",
    "class": "",
    "data-long": LONG_ATTRIBUTE
  });

  let editor = getContainerForRawNode("#node24", inspector).editor;
  let attr = editor.attrs["data-long"].querySelector(".editable");

  // Check to make sure it has expanded after focus
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  let input = inplaceEditor(attr).input;
  is (input.value, 'data-long="' + LONG_ATTRIBUTE + '"');
  EventUtils.sendKey("escape", inspector.panelWin);

  setEditableFieldValue(attr, input.value + ' data-short="ABC"', inspector);
  yield inspector.once("markupmutation");

  let visibleAttrText = editor.attrs["data-long"].querySelector(".attr-value").textContent;
  is (visibleAttrText, LONG_ATTRIBUTE_COLLAPSED)

  assertAttributes("#node24", {
    id: "node24",
    class: "",
    'data-long': LONG_ATTRIBUTE,
    "data-short": "ABC"
  });
}

function* testModifyInlineStyleWithQuotes(inspector) {
  info("Modify inline style containing \"");

  assertAttributes("#node26", {
    id: "node26",
    style: 'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F");'
  });

  let onMutated = inspector.once("markupmutation");
  let editor = getContainerForRawNode("#node26", inspector).editor;
  let attr = editor.attrs["style"].querySelector(".editable");

  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  let input = inplaceEditor(attr).input;
  let value = input.value;

  is (value,
    "style='background-image: url(\"moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F\");'",
    "Value contains actual double quotes"
  );

  value = value.replace(/mozilla\.org/, "mozilla.com");
  input.value = value;

  EventUtils.sendKey("return", inspector.panelWin);

  yield onMutated;

  assertAttributes("#node26", {
    id: "node26",
    style: 'background-image: url("moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.com%2F");'
  });
}

function* testEditingAttributeWithMixedQuotes(inspector) {
  info("Modify class containing \" and \'");

  assertAttributes("#node27", {
    "id": "node27",
    "class": 'Double " and single \''
  });

  let onMutated = inspector.once("markupmutation");
  let editor = getContainerForRawNode("#node27", inspector).editor;
  let attr = editor.attrs["class"].querySelector(".editable");

  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);

  let input = inplaceEditor(attr).input;
  let value = input.value;

  is (value, "class=\"Double &quot; and single '\"", "Value contains &quot;");

  value = value.replace(/Double/, "&quot;").replace(/single/, "'");
  input.value = value;

  EventUtils.sendKey("return", inspector.panelWin);

  yield onMutated;

  assertAttributes("#node27", {
    id: "node27",
    class: '" " and \' \''
  });
}
