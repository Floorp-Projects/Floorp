/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing long classnames shows the whole class attribute without scrollbars.

const classname =
  "this-long-class-attribute-should-be-displayed " +
  "without-overflow-when-switching-to-edit-mode " +
  "AAAAAAAAAAAA-BBBBBBBBBBBBB-CCCCCCCCCCCCC-DDDDDDDDDDDDDD-EEEEEEEEEEEEE";
const TEST_URL = `data:text/html;charset=utf8, <div class="${classname}"></div>`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  await selectNode("div", inspector);
  await clickContainer("div", inspector);

  const container = await focusNode("div", inspector);
  ok(container && container.editor, "The markup-container was found");

  info("Listening for the markupmutation event");
  const nodeMutated = inspector.once("markupmutation");
  const attr = container.editor.attrElements
    .get("class")
    .querySelector(".editable");

  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  const input = inplaceEditor(attr).input;
  ok(input, "Found editable field for class attribute");

  is(
    input.scrollHeight,
    input.clientHeight,
    "input should not have vertical scrollbars"
  );
  is(
    input.scrollWidth,
    input.clientWidth,
    "input should not have horizontal scrollbars"
  );
  input.value = 'class="other value"';

  info("Commit the new class value");
  EventUtils.sendKey("return", inspector.panelWin);

  info("Wait for the markup-mutation event");
  await nodeMutated;
});
