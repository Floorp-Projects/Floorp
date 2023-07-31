/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing editing nested rules selector in the rule view.

const STYLE = `
    h1 {
      color: lime;
      &.foo {
        color: red;
      }
    }`;

const HTML = `<h1 class=foo>Nested</h1>`;

const TEST_URI_INLINE_SHEET = `
  <style>${STYLE}</style>
  ${HTML}`;

const TEST_URI_CONSTRUCTED_SHEET = `
  ${HTML}
  <script>
    const sheet = new CSSStyleSheet();
    sheet.replaceSync(\`${STYLE}\`);
    document.adoptedStyleSheets.push(sheet);
  </script>
`;

add_task(async function test_inline_sheet() {
  info("Run test with inline stylesheet");
  await runTest(TEST_URI_INLINE_SHEET);
});

add_task(async function test_constructed_sheet() {
  info("Run test with constructed stylesheet");
  await runTest(TEST_URI_CONSTRUCTED_SHEET);
});

async function runTest(uri) {
  await addTab(`data:text/html,<meta charset=utf8>${encodeURIComponent(uri)}`);
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);

  is(
    await getComputedStyleProperty("h1", null, "color"),
    "rgb(255, 0, 0)",
    "h1 color is red initially"
  );

  info(`Modify "&.foo" selector into "&.bar"`);
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const editor = await focusEditableField(view, ruleEditor.selectorText);
  const onRuleViewChanged = view.once("ruleview-changed");
  editor.input.value = "&.bar";
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(
    await getComputedStyleProperty("h1", null, "color"),
    "rgb(0, 255, 0)",
    "h1 color is now lime, as the new selector does not match the element"
  );

  info(`Modify color in "h1" rule to blue`);
  await updateDeclaration(view, 2, { color: "lime" }, { color: "blue" });
  is(
    await getComputedStyleProperty("h1", null, "color"),
    "rgb(0, 0, 255)",
    "h1 color is now blue"
  );
}
