/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing editing nested rules in the rule view.

const STYLE = `
    main {
      background-color: tomato;
      & > .foo {
        background-color: teal;
        &.foo {
          color: gold;
        }
      }
    }`;

const HTML = `
  <main>
    Hello
    <div class=foo>Nested</div>
  </main>`;

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

  await selectNode(".foo", inspector);

  info(`Modify color in "&.foo" rule`);
  await updateDeclaration(view, 1, { color: "gold" }, { color: "white" });
  is(
    await getComputedStyleProperty(".foo", null, "color"),
    "rgb(255, 255, 255)",
    "color was set to white on .foo"
  );

  info(`Modify background-color in "& > .foo" rule`);
  await updateDeclaration(
    view,
    2,
    { "background-color": "teal" },
    { "background-color": "blue" }
  );
  is(
    await getComputedStyleProperty(".foo", null, "background-color"),
    "rgb(0, 0, 255)",
    "background-color was set to blue on .foo…"
  );
  is(
    await getComputedStyleProperty(".foo", null, "color"),
    "rgb(255, 255, 255)",
    "…and color is still white"
  );

  await selectNode("main", inspector);
  info(`Modify background-color in "main" rule`);
  await updateDeclaration(
    view,
    1,
    { "background-color": "tomato" },
    { "background-color": "red" }
  );
  is(
    await getComputedStyleProperty("main", null, "background-color"),
    "rgb(255, 0, 0)",
    "background-color was set to red on <main>…"
  );
  is(
    await getComputedStyleProperty(".foo", null, "background-color"),
    "rgb(0, 0, 255)",
    "…background-color is still blue on .foo…"
  );
  is(
    await getComputedStyleProperty(".foo", null, "color"),
    "rgb(255, 255, 255)",
    "…and color is still white"
  );
}
