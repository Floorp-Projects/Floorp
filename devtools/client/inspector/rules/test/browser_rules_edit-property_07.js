/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding multiple values will enable the property even if the
// property does not change, and that the extra values are added correctly.

const STYLE = "#testid { background-color: #f00 }";

const TEST_URI_INLINE_SHEET = `
  <style>${STYLE}</style>
  <div id='testid'>Styled Node</div>
`;

const TEST_URI_CONSTRUCTED_SHEET = `
  <div id='testid'>Styled Node</div>
  <script>
    let sheet = new CSSStyleSheet();
    sheet.replaceSync("${STYLE}");
    document.adoptedStyleSheets.push(sheet);
  </script>
`;

async function runTest(testUri) {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(testUri));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const prop = rule.textProps[0];

  info("Disabling red background color property");
  await togglePropStatus(view, prop);
  ok(!prop.enabled, "red background-color property is disabled.");

  const editor = await focusEditableField(view, prop.editor.valueSpan);
  const onDone = view.once("ruleview-changed");
  editor.input.value = "red; color: red;";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onDone;

  is(
    prop.editor.valueSpan.textContent,
    "red",
    "'red' property value is correctly set."
  );
  ok(prop.enabled, "red background-color property is enabled.");
  is(
    await getComputedStyleProperty("#testid", null, "background-color"),
    "rgb(255, 0, 0)",
    "red background color is set."
  );

  const propEditor = rule.textProps[1].editor;
  is(
    propEditor.nameSpan.textContent,
    "color",
    "new 'color' property name is correctly set."
  );
  is(
    propEditor.valueSpan.textContent,
    "red",
    "new 'red' property value is correctly set."
  );
  is(
    await getComputedStyleProperty("#testid", null, "color"),
    "rgb(255, 0, 0)",
    "red color is set."
  );
}

add_task(async function test_inline_sheet() {
  await runTest(TEST_URI_INLINE_SHEET);
});

add_task(async function test_constructed_sheet() {
  await runTest(TEST_URI_CONSTRUCTED_SHEET);
});
